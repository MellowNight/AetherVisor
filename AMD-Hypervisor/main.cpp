NptHooksTlbHooks#include "itlb_hook.h"
#include "npt_hook.h"
#include "logging.h"
#include "disassembly.h"
#include "prepare_vm.h"

extern "C" void __stdcall LaunchVm(
	void* vm_launch_params
);

bool VirtualizeAllProcessors()
{
	if (!IsSvmSupported())
	{
		Logger::Log("[SETUP] SVM isn't supported on this processor! \n");
		return false;
	}

	if (!IsSvmUnlocked())
	{
		Logger::Log("[SETUP] SVM operation is locked off in BIOS! \n");
		return false;
	}

	hypervisor = (Hypervisor*)ExAllocatePoolZero(NonPagedPool, sizeof(Hypervisor), 'HvDa');

	BuildNestedPagingTables(&hypervisor->normal_ncr3, true);
	BuildNestedPagingTables(&hypervisor->noexecute_ncr3, false);
	BuildNestedPagingTables(&hypervisor->tertiary_cr3, true);

	hypervisor->core_count = KeQueryActiveProcessorCount(0);

	for (int idx = 0; idx < hypervisor->core_count; ++idx)
	{
		KAFFINITY affinity = Utils::Exponent(2, idx);

		KeSetSystemAffinityThread(affinity);

		DbgPrint("=============================================================== \n");
		DbgPrint("[SETUP] amount of active processors %i \n", hypervisor->core_count);
		DbgPrint("[SETUP] Currently running on core %i \n", idx);

		auto reg_context = (CONTEXT*)ExAllocatePoolZero(NonPagedPool, sizeof(CONTEXT), 'Cotx');

		RtlCaptureContext(reg_context);

		if (hypervisor->IsHypervisorPresent(idx) == false)
		{
			EnableSvme();

			auto vcpu_data = hypervisor->vcpu_data;

			vcpu_data[idx] = (CoreVmcbData*)ExAllocatePoolZero(NonPagedPool, sizeof(CoreVmcbData), 'Vmcb');

			ConfigureProcessor(vcpu_data[idx], reg_context);

			SegmentAttribute cs_attrib;

			cs_attrib.as_uint16 = vcpu_data[idx]->guest_vmcb.save_state_area.CsAttrib;

			if (IsProcessorReadyForVmrun(&vcpu_data[idx]->guest_vmcb, cs_attrib))
			{
				DbgPrint("address of guest vmcb save state area = %p \n", &vcpu_data[idx]->guest_vmcb.save_state_area.Rip);

				LaunchVm(&vcpu_data[idx]->guest_vmcb_physicaladdr);
			}
			else
			{
				Logger::Log("[SETUP] A problem occured!! invalid guest state \n");
				__debugbreak();
			}
		}
		else
		{
			DbgPrint("============== Hypervisor Successfully Launched rn !! ===============\n \n");
		}
	}

	LARGE_INTEGER delay = { 30000000 };	// 3 seconds
	KeDelayExecutionThread(KernelMode, FALSE, &delay);

	auto code_page = ExAllocatePool(NonPagedPool, PAGE_SIZE);
	memset(code_page, 0xCC, PAGE_SIZE);
	*(char*)code_page = 0xC3;

	TlbHooker::SetTlbHook(code_page, (uint8_t*)"\xCC\xC3", 2);

	__debugbreak();
	static_cast<void(*)()>(code_page)();

	unsigned char firstbyte = *(unsigned char*)code_page & 0xFF;
	Logger::Log("first byte of code page is %02x \n", firstbyte);
	__debugbreak();
}

NTSTATUS DriverUnload(PDRIVER_OBJECT DriverObject)
{
	Logger::Log("[AMD-Hypervisor] - Devirtualizing system, Driver unloading!\n");

	return STATUS_SUCCESS;
}

NTSTATUS EntryPoint(PDRIVER_OBJECT DriverObject, PUNICODE_STRING RegistryPath)
{
	Logger::Start();
	Disasm::Init();
	TlbHooker::Init();
	NptHooker::Init();

	Logger::Log("EntryPoint \n");

	HANDLE thread_handle;

	PsCreateSystemThread(
		&thread_handle, 
		GENERIC_ALL, NULL, NULL, NULL,
		(PKSTART_ROUTINE)VirtualizeAllProcessors,
		NULL
	);

	return	STATUS_SUCCESS;
}