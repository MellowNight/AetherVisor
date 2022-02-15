#include "virtualization.h"
#include "logging.h"
#include "disassembly.h"

int	core_count = 0;
CoreVmcbData* vcpu_data[32] = { 0 };

Hypervisor* hypervisor = 0;

extern "C" void NTAPI LaunchVm(void* vm_launch_params);

bool IsHypervisorPresent(int32_t core_number)
{
	/*	shitty check, switched from vmmcall to pointer check to avoid #UD	*/

	if (vcpu_data[core_number] != NULL)
	{
		return true;
	}
	else
	{
		return false;
	}
}

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

	core_count = KeQueryActiveProcessorCount(0);

	for (int idx = 0; idx < core_count; ++idx)
	{
		KAFFINITY affinity = Utils::Exponent(2, idx);

		KeSetSystemAffinityThread(affinity);

		Logger::Log(L"============================================================================= \n");
		Logger::Log(L"[SETUP] amount of active processors %i \n", core_count);
		Logger::Log(L"[SETUP] Currently running on core %i \n", idx);

		auto reg_context = (CONTEXT*)ExAllocatePoolZero(NonPagedPool, sizeof(CONTEXT), 'Cotx');

		RtlCaptureContext(reg_context);

		if (IsHypervisorPresent(idx) == false)
		{
			EnableSvme();

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