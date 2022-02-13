#include "virtualization.h"

extern "C" void _sgdt(_Out_ void* Descriptor);

/*	Copy bits bits 55:52 and 47:40	*/
SegmentAttribute GetSegmentAttributes(UINT16 SegmentSelector, uintptr_t gdt_base)
{
	SEGMENT_SELECTOR	seg_selector;

	seg_selector.Flags = SegmentSelector;

	SegmentDescriptor	seg_descriptor = ((SegmentDescriptor*)gdt_base)[seg_selector.Index];

	SegmentAttribute	attribute;

	attribute.Fields.Type = seg_descriptor.Fields.Type;
	attribute.Fields.System = seg_descriptor.Fields.System;
	attribute.Fields.Dpl = seg_descriptor.Fields.Dpl;
	attribute.Fields.Present = seg_descriptor.Fields.Present;
	attribute.Fields.Avl = seg_descriptor.Fields.Avl;
	attribute.Fields.LongMode = seg_descriptor.Fields.LongMode;
	attribute.Fields.DefaultBit = seg_descriptor.Fields.DefaultBit;
	attribute.Fields.Granularity = seg_descriptor.Fields.Granularity;
	attribute.Fields.Reserved1 = 0;

	return attribute;
}

void	ConfigureProcessor(CoreVmcbData* core_data, PCONTEXT ContextRecord, HYPERVISOR_DATA* Hvdata)
{
	core_data->guest_vmcb_physicaladdr = MmGetPhysicalAddress(&core_data->GuestVmcb).QuadPart;
	core_data->host_vmcb_physicaladdr = MmGetPhysicalAddress(&core_data->HostVmcb).QuadPart;
	core_data->Self = core_data;


	core_data->GuestVmcb.ControlArea.NCr3 = g_HvData->PrimaryNCr3;
	core_data->GuestVmcb.ControlArea.NpEnable = (1UL << 0);

	DESCRIPTOR_TABLE_REGISTER	Gdtr, idtr;

	_sgdt(&Gdtr);
	__sidt(&idtr);

	core_data->GuestVmcb.ControlArea.InterceptVec4 |= INTERCEPT_VMMCALL;
	core_data->GuestVmcb.ControlArea.InterceptVec4 |= INTERCEPT_VMRUN;


	core_data->GuestVmcb.ControlArea.GuestAsid = 1;
	core_data->GuestVmcb.SaveStateArea.Cr0 = __readcr0();
	core_data->GuestVmcb.SaveStateArea.Cr2 = __readcr2();
	core_data->GuestVmcb.SaveStateArea.Cr3 = __readcr3();
	core_data->GuestVmcb.SaveStateArea.Cr4 = __readcr4();


	core_data->GuestVmcb.SaveStateArea.Rip = ContextRecord->Rip;
	core_data->GuestVmcb.SaveStateArea.Rax = ContextRecord->Rax;
	core_data->GuestVmcb.SaveStateArea.Rsp = ContextRecord->Rsp;
	core_data->GuestVmcb.SaveStateArea.Rflags = __readeflags();
	core_data->GuestVmcb.SaveStateArea.Efer = __readmsr(AMD_EFER);
	core_data->GuestVmcb.SaveStateArea.GPat = __readmsr(AMD_MSR_PAT);

	core_data->GuestVmcb.SaveStateArea.GdtrLimit = Gdtr.Limit;
	core_data->GuestVmcb.SaveStateArea.GdtrBase = Gdtr.Base;
	core_data->GuestVmcb.SaveStateArea.IdtrLimit = idtr.Limit;
	core_data->GuestVmcb.SaveStateArea.IdtrBase = idtr.Base;

	core_data->GuestVmcb.SaveStateArea.CsLimit = GetSegmentLimit(ContextRecord->SegCs);
	core_data->GuestVmcb.SaveStateArea.DsLimit = GetSegmentLimit(ContextRecord->SegDs);
	core_data->GuestVmcb.SaveStateArea.EsLimit = GetSegmentLimit(ContextRecord->SegEs);
	core_data->GuestVmcb.SaveStateArea.SsLimit = GetSegmentLimit(ContextRecord->SegSs);

	core_data->GuestVmcb.SaveStateArea.CsSelector = ContextRecord->SegCs;
	core_data->GuestVmcb.SaveStateArea.DsSelector = ContextRecord->SegDs;
	core_data->GuestVmcb.SaveStateArea.EsSelector = ContextRecord->SegEs;
	core_data->GuestVmcb.SaveStateArea.SsSelector = ContextRecord->SegSs;


	core_data->GuestVmcb.SaveStateArea.CsAttrib = GetSegmentAttributes(ContextRecord->SegCs, Gdtr.Base).AsUInt16;
	core_data->GuestVmcb.SaveStateArea.DsAttrib = GetSegmentAttributes(ContextRecord->SegDs, Gdtr.Base).AsUInt16;
	core_data->GuestVmcb.SaveStateArea.EsAttrib = GetSegmentAttributes(ContextRecord->SegEs, Gdtr.Base).AsUInt16;
	core_data->GuestVmcb.SaveStateArea.SsAttrib = GetSegmentAttributes(ContextRecord->SegSs, Gdtr.Base).AsUInt16;

	DbgPrint("VpData->GuestVmcb: %p\n", core_data->GuestVmcb);
	DbgPrint("VpData->GuestVmcbPa: %p\n", core_data->guest_vmcb_physicaladdr);

	__svm_vmsave(core_data->guest_vmcb_physicaladdr);

	__writemsr(VM_HSAVE_PA, MmGetPhysicalAddress(&core_data->HostSaveArea).QuadPart);

	__svm_vmsave(core_data->host_vmcb_physicaladdr);
}

bool	IsSvmSupported()
{
	int	cpuInfo[4] = { 0 };

	/*	  CPUID_Fn80000001_ECX.bit_2	*/
	__cpuid(cpuInfo, CPUID_PROCESSOR_AND_PROCESSOR_FEATURE_IDENTIFIERS);

	if ((cpuInfo[2] & (1 << 1)) == 0)
	{
		return false;
	}

	int		VendorNameResult[4];

	char	VendorName[13];

	__cpuid(VendorNameResult, CPUID_MAX_STANDARD_FN_NUMBER_AND_VENDOR_STRING);
	memcpy(VendorName, &VendorNameResult[1], sizeof(int));
	memcpy(VendorName + 4, &VendorNameResult[3], sizeof(int));
	memcpy(VendorName + 8, &VendorNameResult[2], sizeof(int));

	VendorName[12] = '\0';

	DbgPrint("[SETUP] Vendor Name %s \n", VendorName);

	if (strcmp(VendorName, "AuthenticAMD") && strcmp(VendorName, "VmwareVmware"))
	{
		return false;
	}

	return true;
}

bool	IsSvmUnlocked()
{
	VM_CR_MSR	msr;
	msr.Flags = __readmsr(AMD_VM_CR);


	if (msr.SVMLock == 0)
	{
		msr.SVMEDisable = 0;
		msr.SVMLock = 1;
		__writemsr(AMD_VM_CR, msr.Flags);
	}
	else if (msr.SVMEDisable == 1)
	{
		return false;
	}

	return true;
}

void	EnableSVME()
{
	EFER_MSR	msr;
	msr.Flags = __readmsr(AMD_EFER);
	msr.SVME = 1;
	__writemsr(AMD_EFER, msr.Flags);
}