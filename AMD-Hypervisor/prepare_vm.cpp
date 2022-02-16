#include "prepare_vm.h"
#include "logging.h"



extern "C" void _sgdt(
	OUT void* Descriptor
);

bool IsProcessorReadyForVmrun(Vmcb* guest_vmcb, SegmentAttribute cs_attribute)
{
	MsrEfer efer_msr = { 0 };
	efer_msr.flags = __readmsr(MSR::EFER);

	if (efer_msr.svme == 1)
	{
		Logger::Log(L"SVME is off, invalid state! \n");
		return false;
	}

	if ((efer_msr.reserved2 != 0) || (efer_msr.reserved3 != 0) || (efer_msr.reserved4 != 0))
	{
		Logger::Log(L"MBZ bit of EFER is set, Invalid state! \n");
		return false;
	}

	CR0	cr0;
	cr0.Flags = __readcr0();

	if ((cr0.CacheDisable == 0) && (cr0.NotWriteThrough == 1))
	{
		Logger::Log(L"CR0.CD is zero and CR0.NW is set. \n");
		return false;
	}

	if (cr0.Reserved4 != 0)
	{
		Logger::Log(L"CR0[63:32] are not zero. \n");
		return false;
	}
	
	RFLAGS rflags;
	rflags.Flags = __readeflags();

	CR3	cr3;
	CR4	cr4;

	cr3.Flags = __readcr3();
	cr4.Flags = __readcr4();

	if (rflags.Virtual8086ModeFlag == 1 && (cr4.Flags << 23 & 1))
	{
		Logger::Log(L"CR4.CET=1 and U_CET.SS=1 when EFLAGS.VM=1 \n");
	}

	if ((cr3.Reserved1 != 0) || (cr3.Reserved2 != 0) || (cr4.Reserved1 != 0)
		|| (cr4.Reserved2 != 0) || (cr4.Reserved3 != 0) || (cr4.Reserved4 != 0))
	{
		Logger::Log(L"cr3 or cr4 MBZ bits are zero. Invalid state rn \n");
		return false;
	}

	DR6	dr6;
	DR7 dr7;

	dr6.Flags = __readdr(6);
	dr7.Flags = __readdr(7);

	if ((dr6.Flags & (0xFFFFFFFF00000000)) || (dr7.Reserved4 != 0))
	{
		Logger::Log(L"DR6[63:32] are not zero, or DR7[63:32] are not zero. Invalid State! \n");
		return false;
	}

	if (cr0.PagingEnable == 0)
	{
		Logger::Log(L"Paging disabled, Invalid state! \n");
		return false;
	}

	if (efer_msr.long_mode_enable == 1 && cr0.PagingEnable == 1)
	{
		if (cr4.PhysicalAddressExtension == 0)
		{
			Logger::Log(L"EFER.LME and CR0.PG are both set and CR4.PAE is zero, Invalid state! \n");
			return false;
		}

		if (cr0.ProtectionEnable == 0)
		{
			Logger::Log(L"EFER.LME and CR0.PG are both non-zero and CR0.PE is zero, Invalid state! \n");
			return false;
		}

		if (cs_attribute.fields.long_mode != 0 && cs_attribute.fields.long_mode != 0)
		{
			Logger::Log(L"EFER.LME, CR0.PG, CR4.PAE, CS.L, and CS.D are all non-zero. \n");
			return false;
		}
	}

	if (guest_vmcb->control_area.GuestAsid == 0)
	{
		Logger::Log(L"ASID is equal to zero. Invalid guest state \n");
		return false;
	}

	if (!(guest_vmcb->control_area.InterceptVec4 & 1))
	{
		Logger::Log(L"The VMRUN intercept bit is clear. Invalid state! \n");
		return false;
	}

	Logger::Log(L"consistency checks passed \n");
	return true;

	/*	to do: msr and IOIO map address checks, and some more. */
}

/*	Copy bits bits 55:52 and 47:40 from segment descriptor	*/
SegmentAttribute GetSegmentAttributes(uint16_t segment_selector, uintptr_t gdt_base)
{
	auto selector = SEGMENT_SELECTOR{ segment_selector };

	SegmentDescriptor	seg_descriptor = ((SegmentDescriptor*)gdt_base)[selector.Index];

	SegmentAttribute	attribute;

	attribute.fields.type = seg_descriptor.Fields.Type;
	attribute.fields.system = seg_descriptor.Fields.System;
	attribute.fields.dpl = seg_descriptor.Fields.Dpl;
	attribute.fields.present = seg_descriptor.Fields.Present;
	attribute.fields.avl = seg_descriptor.Fields.Avl;
	attribute.fields.long_mode = seg_descriptor.Fields.LongMode;
	attribute.fields.default_bit = seg_descriptor.Fields.DefaultBit;
	attribute.fields.granularity = seg_descriptor.Fields.Granularity;
	attribute.fields.reserved1 = 0;

	return attribute;
}

void ConfigureProcessor(CoreVmcbData* core_data, CONTEXT* context_record)
{
	core_data->guest_vmcb_physicaladdr = MmGetPhysicalAddress(&core_data->guest_vmcb).QuadPart;
	core_data->host_vmcb_physicaladdr = MmGetPhysicalAddress(&core_data->host_vmcb).QuadPart;
	core_data->self = core_data;

	core_data->guest_vmcb.control_area.NCr3 = hypervisor->normal_ncr3;
	core_data->guest_vmcb.control_area.NpEnable = (1UL << 0);

	DescriptorTableRegister	gdtr, idtr;

	_sgdt(&gdtr);
	__sidt(&idtr);

	InterceptVector4 intercept_vector4;

	intercept_vector4.intercept_vmmcall = 1; 
	intercept_vector4.intercept_vmrun = 1;

	core_data->guest_vmcb.control_area.InterceptVec4 = intercept_vector4.as_int32;


	core_data->guest_vmcb.control_area.GuestAsid = 1;
	core_data->guest_vmcb.save_state_area.Cr0 = __readcr0();
	core_data->guest_vmcb.save_state_area.Cr2 = __readcr2();
	core_data->guest_vmcb.save_state_area.Cr3 = __readcr3();
	core_data->guest_vmcb.save_state_area.Cr4 = __readcr4();


	core_data->guest_vmcb.save_state_area.Rip = context_record->Rip;
	core_data->guest_vmcb.save_state_area.Rax = context_record->Rax;
	core_data->guest_vmcb.save_state_area.Rsp = context_record->Rsp;
	core_data->guest_vmcb.save_state_area.Rflags = __readeflags();
	core_data->guest_vmcb.save_state_area.Efer = __readmsr(MSR::EFER);
	core_data->guest_vmcb.save_state_area.GPat = __readmsr(MSR::PAT);

	core_data->guest_vmcb.save_state_area.GdtrLimit = gdtr.limit;
	core_data->guest_vmcb.save_state_area.GdtrBase = gdtr.base;
	core_data->guest_vmcb.save_state_area.IdtrLimit = idtr.limit;
	core_data->guest_vmcb.save_state_area.IdtrBase = idtr.base;

	core_data->guest_vmcb.save_state_area.CsLimit = GetSegmentLimit(context_record->SegCs);
	core_data->guest_vmcb.save_state_area.DsLimit = GetSegmentLimit(context_record->SegDs);
	core_data->guest_vmcb.save_state_area.EsLimit = GetSegmentLimit(context_record->SegEs);
	core_data->guest_vmcb.save_state_area.SsLimit = GetSegmentLimit(context_record->SegSs);

	core_data->guest_vmcb.save_state_area.CsSelector = context_record->SegCs;
	core_data->guest_vmcb.save_state_area.DsSelector = context_record->SegDs;
	core_data->guest_vmcb.save_state_area.EsSelector = context_record->SegEs;
	core_data->guest_vmcb.save_state_area.SsSelector = context_record->SegSs;

	core_data->guest_vmcb.save_state_area.CsAttrib = GetSegmentAttributes(context_record->SegCs, gdtr.base).as_uint16;
	core_data->guest_vmcb.save_state_area.DsAttrib = GetSegmentAttributes(context_record->SegDs, gdtr.base).as_uint16;
	core_data->guest_vmcb.save_state_area.EsAttrib = GetSegmentAttributes(context_record->SegEs, gdtr.base).as_uint16;
	core_data->guest_vmcb.save_state_area.SsAttrib = GetSegmentAttributes(context_record->SegSs, gdtr.base).as_uint16;

	Logger::Log(L"VpData->guest_vmcb: %p\n", core_data->guest_vmcb);
	Logger::Log(L"VpData->guest_vmcbPa: %p\n", core_data->guest_vmcb_physicaladdr);

	__svm_vmsave(core_data->guest_vmcb_physicaladdr);

	__writemsr(VM_HSAVE_PA, MmGetPhysicalAddress(&core_data->host_save_area).QuadPart);

	__svm_vmsave(core_data->host_vmcb_physicaladdr);
}

bool IsSvmSupported()
{
	int32_t	cpu_info[4] = { 0 };

	__cpuid(cpu_info, CPUID::PROCESSOR_AND_FEATURE_IDENTIFIERS);

	if ((cpu_info[2] & (1 << 1)) == 0)
	{
		return false;
	}

	int32_t vendor_name_result[4];

	char vendor_name[13];

	__cpuid(vendor_name_result, CPUID::MAX_STANDARD_FN_NUMBER_AND_VENDOR_STRING);
	memcpy(vendor_name, &vendor_name_result[1], sizeof(int));
	memcpy(vendor_name + 4, &vendor_name_result[3], sizeof(int));
	memcpy(vendor_name + 8, &vendor_name_result[2], sizeof(int));

	vendor_name[12] = '\0';

	Logger::Log(L"[SETUP] Vendor Name %s \n", vendor_name);

	if (strcmp(vendor_name, "AuthenticAMD") && strcmp(vendor_name, "VmwareVmware"))
	{
		return false;
	}

	return true;
}

bool IsSvmUnlocked()
{
	MsrVmcr	msr;
	msr.flags = __readmsr(MSR::VM_CR);


	if (msr.svm_lock == 0)
	{
		msr.svme_disable = 0;
		msr.svm_lock = 1;
		__writemsr(MSR::VM_CR, msr.flags);
	}
	else if (msr.svme_disable == 1)
	{
		return false;
	}

	return true;
}

void EnableSvme()
{
	MsrEfer	msr;
	msr.flags = __readmsr(MSR::EFER);
	msr.svme = 1;
	__writemsr(MSR::EFER, msr.flags);
}