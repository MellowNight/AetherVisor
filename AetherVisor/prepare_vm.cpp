#include "prepare_vm.h"
#include "logging.h"


/*	Copy bits bits 55:52 and 47:40 from segment descriptor	*/
SegmentAttribute GetSegmentAttributes(uint16_t segment_selector, uintptr_t gdt_base)
{
	SEGMENT_SELECTOR selector; selector.Flags = segment_selector;

	SegmentDescriptor seg_descriptor = ((SegmentDescriptor*)gdt_base)[selector.Index];

	SegmentAttribute attribute;

	attribute.fields.type = seg_descriptor.Type;
	attribute.fields.system = seg_descriptor.System;
	attribute.fields.dpl = seg_descriptor.Dpl;
	attribute.fields.present = seg_descriptor.Present;
	attribute.fields.avl = seg_descriptor.Avl;
	attribute.fields.long_mode = seg_descriptor.LongMode;
	attribute.fields.default_bit = seg_descriptor.DefaultBit;
	attribute.fields.granularity = seg_descriptor.Granularity;
	attribute.fields.reserved1 = 0;

	return attribute;
}

void SetupMSRPM(VcpuData* vcpu)
{
	size_t bits_per_msr = 16000 / 8000;
	size_t bits_per_byte = sizeof(uint8_t) * 8;
	size_t msrpm_size = PAGE_SIZE * 2;

	auto msrpm = ExAllocatePoolZero(NonPagedPool, msrpm_size, 'msr0');

	vcpu->guest_vmcb.control_area.msrpm_base_pa = MmGetPhysicalAddress(msrpm).QuadPart;

	RTL_BITMAP bitmap;

    RtlInitializeBitMap(&bitmap, (PULONG)msrpm, msrpm_size * 8);

    RtlClearAllBits(&bitmap);

	auto section2_offset = (0x800 * bits_per_byte);

	auto efer_offset = section2_offset + (bits_per_msr * (MSR::efer - 0xC0000000));

	/*	intercept EFER read and write	*/

	RtlSetBits(&bitmap, efer_offset, 2);
}

void VcpuData::ConfigureProcessor(CONTEXT* context_record)
{
	guest_vmcb_physicaladdr = MmGetPhysicalAddress(&guest_vmcb).QuadPart;
	host_vmcb_physicaladdr = MmGetPhysicalAddress(&host_vmcb).QuadPart;

	self = this;

	guest_vmcb.control_area.ncr3 = Hypervisor::Get()->ncr3_dirs[primary];
	guest_vmcb.control_area.np_enable = (1UL << 0);
	guest_vmcb.control_area.lbr_virtualization_enable |= (1UL << 0);

	DescriptorTableRegister	gdtr, idtr;

	_sgdt(&gdtr);
	__sidt(&idtr);

	InterceptVector4 intercept_vector4;

	intercept_vector4.intercept_vmmcall = 1;
	intercept_vector4.intercept_vmrun = 1;

	guest_vmcb.control_area.intercept_vec4 = intercept_vector4.as_int32;

	InterceptVector2 intercept_vector2 = { 0 };

	intercept_vector2.intercept_bp = 1;
	intercept_vector2.intercept_db = 1;

	guest_vmcb.control_area.intercept_exception = intercept_vector2.as_int32; 

	/*	intercept MSR access	*/
	
	guest_vmcb.control_area.intercept_vec3 |= (1UL << 28);

	guest_vmcb.control_area.guest_asid = 1;

	guest_vmcb.save_state_area.cr0.Flags = __readcr0();
	guest_vmcb.save_state_area.cr2 = __readcr2();
	guest_vmcb.save_state_area.cr3.Flags = __readcr3();
	guest_vmcb.save_state_area.cr4.Flags = __readcr4();

	guest_vmcb.save_state_area.rip = context_record->Rip;
	guest_vmcb.save_state_area.rax = context_record->Rax;
	guest_vmcb.save_state_area.rsp = context_record->Rsp;

	guest_vmcb.save_state_area.efer = __readmsr(MSR::efer);
	guest_vmcb.save_state_area.guest_pat = __readmsr(MSR::pat);

	guest_vmcb.save_state_area.gdtr_limit = gdtr.limit;
	guest_vmcb.save_state_area.gdtr_base = gdtr.base;
	guest_vmcb.save_state_area.idtr_limit = idtr.limit;
	guest_vmcb.save_state_area.idtr_base = idtr.base;

	guest_vmcb.save_state_area.cs_limit = GetSegmentLimit(context_record->SegCs);
	guest_vmcb.save_state_area.ds_limit = GetSegmentLimit(context_record->SegDs);
	guest_vmcb.save_state_area.es_limit = GetSegmentLimit(context_record->SegEs);
	guest_vmcb.save_state_area.ss_limit = GetSegmentLimit(context_record->SegSs);
	
	guest_vmcb.save_state_area.cs_selector = context_record->SegCs;
	guest_vmcb.save_state_area.ds_selector = context_record->SegDs;
	guest_vmcb.save_state_area.es_selector = context_record->SegEs;
	guest_vmcb.save_state_area.ss_selector = context_record->SegSs;

	guest_vmcb.save_state_area.rflags.Flags = __readeflags();
	guest_vmcb.save_state_area.dr7.Flags = __readdr(7);
	guest_vmcb.save_state_area.dbg_ctl.Flags = __readmsr(IA32_DEBUGCTL);

	guest_vmcb.save_state_area.cs_attrib = GetSegmentAttributes(context_record->SegCs, gdtr.base).as_uint16;
	guest_vmcb.save_state_area.ds_attrib = GetSegmentAttributes(context_record->SegDs, gdtr.base).as_uint16;
	guest_vmcb.save_state_area.es_attrib = GetSegmentAttributes(context_record->SegEs, gdtr.base).as_uint16;
	guest_vmcb.save_state_area.ss_attrib = GetSegmentAttributes(context_record->SegSs, gdtr.base).as_uint16;

	SetupMSRPM(this);

	// SetupTssIst();
	
	__svm_vmsave(guest_vmcb_physicaladdr);

	__writemsr(vm_hsave_pa, MmGetPhysicalAddress(&host_save_area).QuadPart);

	__svm_vmsave(host_vmcb_physicaladdr);
}


bool IsCoreReadyForVmrun(VMCB* guest_vmcb, SegmentAttribute cs_attribute)
{
	if (cs_attribute.fields.long_mode == 1)
	{
		DbgPrint("Long mode enabled\n");
	}
	else
	{
		DbgPrint("Long mode disabled\n");

	}

	EferMsr efer_msr = { 0 };
	efer_msr.flags = __readmsr(MSR::efer);

	if (efer_msr.svme == (uint32_t)0)
	{
		DbgPrint("SVME is %p, invalid state! \n", efer_msr.svme);
		return false;
	}

	if ((efer_msr.reserved2 != 0) || (efer_msr.reserved3 != 0) || (efer_msr.reserved4 != 0))
	{
		DbgPrint("MBZ bit of EFER is set, Invalid state! \n");
		return false;
	}

	CR0	cr0;
	cr0.Flags = __readcr0();

	if ((cr0.CacheDisable == 0) && (cr0.NotWriteThrough == 1))
	{
		DbgPrint("CR0.CD is zero and CR0.NW is set. \n");
		return false;
	}

	if (cr0.Reserved4 != 0)
	{
		DbgPrint("CR0[63:32] are not zero. \n");
		return false;
	}

	RFLAGS rflags;
	rflags.Flags = __readeflags();

	CR3	cr3;
	CR4	cr4;

	cr3.Flags = __readcr3();
	cr4.Flags = __readcr4();

	//cr4.Flags &= ~(1UL << 23);
	__writecr4(cr4.Flags);

	if (rflags.Virtual8086ModeFlag == 1 && ((cr4.Flags << 23) & 1))
	{
		DbgPrint("CR4.CET=1 and U_CET.SS=1 when EFLAGS.VM=1 \n");
	}

	if ((cr3.Reserved1 != 0) || (cr3.Reserved2 != 0) || (cr4.Reserved1 != 0)
		|| (cr4.Reserved2 != 0) || (cr4.Reserved3 != 0) || (cr4.Reserved4 != 0))
	{
		cr3.Reserved1 = 0;
		cr3.Reserved2 = 0;
		cr4.Reserved1 = 0;
		cr4.Reserved2 = 0;
		cr4.Reserved3 = 0;
		cr4.Reserved4 = 0;


		__writecr3(cr3.Flags);
		__writecr4(cr4.Flags);
	}

	if ((cr3.Reserved1 != 0) || (cr3.Reserved2 != 0) || (cr4.Reserved1 != 0)
		|| (cr4.Reserved2 != 0) || (cr4.Reserved3 != 0) || (cr4.Reserved4 != 0))
	{
		DbgPrint("cr3 or cr4 MBZ bits are zero. Invalid state rn \n");
		return false;
	}

	DR6	dr6;
	DR7 dr7;

	dr6.Flags = __readdr(6);
	dr7.Flags = __readdr(7);

	if ((dr6.Flags & (0xFFFFFFFF00000000)) || (dr7.Reserved4 != 0))
	{
		dr6.Reserved2 = NULL;
		dr7.Reserved4 = NULL;
	}

	__writedr(6, dr6.Flags);
	__writedr(7, dr7.Flags);

	if ((dr6.Flags & (0xFFFFFFFF00000000)) || (dr7.Reserved4 != 0))
	{
		DbgPrint("DR6[63:32] are not zero, or DR7[63:32] are not zero.Invalid State!\n");
		return false;
	}

	if (cr0.PagingEnable == 0)
	{
		DbgPrint("Paging disabled, Invalid state! \n");
		return false;
	}

	if (efer_msr.long_mode_enable == 1 && cr0.PagingEnable == 1)
	{
		if (cr4.PhysicalAddressExtension == 0)
		{
			Logger::Get()->Get()->Log("EFER.LME and CR0.PG are both set and CR4.PAE is zero, Invalid state! \n");
			return false;
		}

		if (cr0.ProtectionEnable == 0)
		{
			Logger::Get()->Get()->Log("EFER.LME and CR0.PG are both non-zero and CR0.PE is zero, Invalid state! \n");
			return false;
		}
	}

	if (guest_vmcb->control_area.guest_asid == 0)
	{
		DbgPrint("ASID is equal to zero. Invalid guest state \n");
		return false;
	}

	if (!(guest_vmcb->control_area.intercept_vec4 & 1))
	{
		DbgPrint("The VMRUN intercept bit is clear. Invalid state! \n");
		return false;
	}

	DbgPrint("consistency checks passed \n");

	return true;

	/*	to do: msr and IOIO map address checks, and some more. */
}

bool IsSvmSupported()
{
	int32_t	cpu_info[4] = { 0 };

	__cpuid(cpu_info, CPUID::feature_identifier);

	if ((cpu_info[2] & (1 << 2)) == 0)
	{
		return false;
	}

	int32_t vendor_name_result[4];

	char vendor_name[13];

	__cpuid(vendor_name_result, CPUID::vendor_and_max_standard_fn_number);

	memcpy(vendor_name, &vendor_name_result[1], sizeof(int));
	memcpy(vendor_name + 4, &vendor_name_result[3], sizeof(int));
	memcpy(vendor_name + 8, &vendor_name_result[2], sizeof(int));

	vendor_name[12] = '\0';

	DbgPrint("[SETUP] Vendor Name %s \n", vendor_name);

	if (strcmp(vendor_name, "AuthenticAMD") && strcmp(vendor_name, "VmwareVmware"))
	{
		return false;
	}

	return true;
}

bool IsSvmUnlocked()
{
	MsrVmcr	msr;

	msr.flags = __readmsr(MSR::vm_cr);

	if (msr.svm_lock == 0)
	{
		msr.svme_disable = 0;
		msr.svm_lock = 1;
		__writemsr(MSR::vm_cr, msr.flags);
	}
	else if (msr.svme_disable == 1)
	{
		return false;
	}

	return true;
}

void EnableSvme()
{
	EferMsr	msr;
	msr.flags = __readmsr(MSR::efer);
	msr.svme = 1;
	__writemsr(MSR::efer, msr.flags);
}