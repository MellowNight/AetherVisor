#include "prepare_vm.h"
#include "logging.h"

extern "C" void _sgdt(
	OUT void* Descriptor
);

extern "C" int16_t __readtr();

bool IsProcessorReadyForVmrun(VMCB* guest_vmcb, SegmentAttribute cs_attribute)
{
	if (cs_attribute.fields.long_mode == 1)
	{
		DbgPrint("Long mode enabled\n");
	}
	else
	{
		DbgPrint("Long mode disabled\n");

	}

	MsrEfer efer_msr = { 0 };
	efer_msr.flags = __readmsr(MSR::EFER);

	if (efer_msr.svme == (uint32_t)0)
	{
		Logger::Get()->Log("SVME is %p, invalid state! \n", efer_msr.svme);
		return false;
	}

	if ((efer_msr.reserved2 != 0) || (efer_msr.reserved3 != 0) || (efer_msr.reserved4 != 0))
	{
		Logger::Get()->Log("MBZ bit of EFER is set, Invalid state! \n");
		return false;
	}

	CR0	cr0;
	cr0.Flags = __readcr0();

	if ((cr0.CacheDisable == 0) && (cr0.NotWriteThrough == 1))
	{
		Logger::Get()->Log("CR0.CD is zero and CR0.NW is set. \n");
		return false;
	}

	if (cr0.Reserved4 != 0)
	{
		Logger::Get()->Log("CR0[63:32] are not zero. \n");
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
		Logger::Get()->Log("CR4.CET=1 and U_CET.SS=1 when EFLAGS.VM=1 \n");
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
		Logger::Get()->Log("cr3 or cr4 MBZ bits are zero. Invalid state rn \n");
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
		Logger::Get()->Log("DR6[63:32] are not zero, or DR7[63:32] are not zero.Invalid State!\n");
		return false;
	}

	if (cr0.PagingEnable == 0)
	{
		Logger::Get()->Log("Paging disabled, Invalid state! \n");
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

	if (guest_vmcb->control_area.GuestAsid == 0)
	{
		Logger::Get()->Log("ASID is equal to zero. Invalid guest state \n");
		return false;
	}

	if (!(guest_vmcb->control_area.InterceptVec4 & 1))
	{
		Logger::Get()->Log("The VMRUN intercept bit is clear. Invalid state! \n");
		return false;
	}

	Logger::Get()->Log("consistency checks passed \n");
	return true;

	/*	to do: msr and IOIO map address checks, and some more. */
}

/*	Copy bits bits 55:52 and 47:40 from segment descriptor	*/
SegmentAttribute GetSegmentAttributes(uint16_t segment_selector, uintptr_t gdt_base)
{
	SEGMENT_SELECTOR selector;
	selector.Flags = segment_selector;

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

void SetupTssIst()
{	
	DescriptorTableRegister	gdtr;
	_sgdt(&gdtr);

	SEGMENT_SELECTOR selector;
	selector.Flags = __readtr();

	auto seg_descriptor = *(SEGMENT_DESCRIPTOR_64*)(
		gdtr.base + (selector.Index * 8));

	auto base = (uintptr_t)(
		seg_descriptor.BaseAddressLow +
		(seg_descriptor.BaseAddressMiddle << SEGMENT__BASE_ADDRESS_MIDDLE_BIT) +
		(seg_descriptor.BaseAddressHigh << SEGMENT__BASE_ADDRESS_HIGH_BIT)
	);

	if (!seg_descriptor.DescriptorType)
	{
		base += ((uint64_t)seg_descriptor.BaseAddressUpper << 32);
	}

	auto tss_base = (TaskStateSegment*)base;

	/*	Find free IST entry for page fault	*/

	int idx = 0;
	for (idx = 0; idx < 7; ++idx)
	{
		DbgPrint("idx %d \n", idx);

		if (!tss_base->ist[idx])
		{
			break;
		}
	}

	auto pf_stack = ExAllocatePoolZero(NonPagedPool, PAGE_SIZE * 16, 'abcd');

	tss_base->ist[idx] = (uint8_t*)pf_stack + PAGE_SIZE * 16;

	DescriptorTableRegister	idtr;
	__sidt(&idtr);

	DbgPrint("IDT base %p \n", idtr.base);
	DbgPrint("page fault stack %p \n", pf_stack);
	DbgPrint("&((InterruptDescriptor64*)idtr.base)[0xE] %p \n", &((InterruptDescriptor64*)idtr.base)[0xE]);


	auto irql = Utils::DisableWP();

	((InterruptDescriptor64*)idtr.base)[0xE].ist = idx; 

	Utils::EnableWP(irql);
	__debugbreak();
}

void SetupMSRPM(CoreData* core_data)
{
	size_t bits_per_msr = 16000 / 8000;
	size_t bits_per_byte = sizeof(uint8_t) * 8;
	size_t msrpm_size = PAGE_SIZE * 2;

	auto msrpm = ExAllocatePoolZero(NonPagedPool, msrpm_size, 'msr0');

	core_data->guest_vmcb.control_area.MsrpmBasePa = MmGetPhysicalAddress(msrpm).QuadPart;

	RTL_BITMAP bitmap;

    RtlInitializeBitMap(&bitmap, (PULONG)msrpm, msrpm_size * 8);
    RtlClearAllBits(&bitmap);

	auto section2_offset = (0x800 * bits_per_byte);
	auto efer_offset = section2_offset + (bits_per_msr * (MSR::EFER - 0xC0000000));

	/*	intercept EFER read and write	*/
	RtlSetBits(&bitmap, efer_offset, 2);
}

void ConfigureProcessor(CoreData* core_data, CONTEXT* context_record)
{
	core_data->guest_vmcb_physicaladdr = MmGetPhysicalAddress(&core_data->guest_vmcb).QuadPart;
	core_data->host_vmcb_physicaladdr = MmGetPhysicalAddress(&core_data->host_vmcb).QuadPart;
	core_data->self = core_data;

	core_data->guest_vmcb.control_area.NCr3 = Hypervisor::Get()->normal_ncr3;
	core_data->guest_vmcb.control_area.NpEnable = (1UL << 0);

	DescriptorTableRegister	gdtr, idtr;

	_sgdt(&gdtr);
	__sidt(&idtr);

	InterceptVector4 intercept_vector4;

	intercept_vector4.intercept_vmmcall = 1;
	intercept_vector4.intercept_vmrun = 1;

	core_data->guest_vmcb.control_area.InterceptVec4 = intercept_vector4.as_int32;

	InterceptVector2 intercept_vector2 = {0};

	// intercept_vector2.intercept_pf = 1;
	// intercept_vector2.intercept_bp = 1;

	/*	intercept MSR access	*/
	core_data->guest_vmcb.control_area.InterceptVec3 |= (1UL << 28);

	core_data->guest_vmcb.control_area.InterceptException = intercept_vector2.as_int32;
	
	core_data->guest_vmcb.control_area.GuestAsid = 1;


	/*	disable the fucking CET	*/
	CR4 cr4;
	cr4.Flags = __readcr4();
///	cr4.Flags &= ~(1UL << 23);
	__writecr4(cr4.Flags);

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

	SetupMSRPM(core_data);

	// SetupTssIst();
	
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

	Logger::Get()->Log("[SETUP] Vendor Name %s \n", vendor_name);

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