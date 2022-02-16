#include "itlb_hook.h"
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
		Logger::Log(L"[SETUP] SVM isn't supported on this processor! \n");
		return false;
	}

	if (!IsSvmUnlocked())
	{
		Logger::Log(L"[SETUP] SVM operation is locked off in BIOS! \n");
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

		Logger::Log(L"============================================================================= \n");
		Logger::Log(L"[SETUP] amount of active processors %i \n", hypervisor->core_count);
		Logger::Log(L"[SETUP] Currently running on core %i \n", idx);

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
				LaunchVm(&vcpu_data[idx]->guest_vmcb_physicaladdr);
			}
			else
			{
				Logger::Log(L"[SETUP] A problem occured!! invalid guest state \n");
				__debugbreak();
			}
		}
		else
		{
			Logger::Log(L"===================== Hypervisor Successfully Launched rn !! =============================\n \n");
		}
	}

	/*	I had strange crashing issues when I just let the thread return normally	*/

	PsTerminateSystemThread(STATUS_SUCCESS);
}

NTSTATUS DriverUnload(PDRIVER_OBJECT DriverObject)
{
	Logger::Log(L"[AMD-Hypervisor] - Devirtualizing system, Driver unloading!\n");

	return STATUS_SUCCESS;
}

NTSTATUS EntryPoint(PDRIVER_OBJECT DriverObject, PUNICODE_STRING RegistryPath)
{
	Logger::Start();
	Disasm::Init();
	TlbHooker::Init();
	NptHooker::Init();

	Logger::Log(L"EntryPoint \n");

	HANDLE thread_handle;

	PsCreateSystemThread(
		&thread_handle, 
		GENERIC_ALL, NULL, NULL, NULL,
		(PKSTART_ROUTINE)VirtualizeAllProcessors,
		NULL
	);

	return	STATUS_SUCCESS;
}