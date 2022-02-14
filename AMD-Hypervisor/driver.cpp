#include "virtualization.h"

int	core_count = 0;
CoreVmcbData* hv_core_data[32] = { 0 };

extern "C" void NTAPI LaunchVm(void* vm_launch_params);

bool IsHypervisorPresent(int32_t core_number)
{
	/*	shitty check, switched from vmmcall to pointer check to avoid #UD	*/

	if (hv_core_data[core_number] != NULL)
	{
		return true;
	}
	else
	{
		return false;
	}
}

GlobalHvData* global_hypervisor_data = 0;

bool VirtualizeAllProcessors()
{
	if (!IsSvmSupported())
	{
		KeBugCheck(MANUALLY_INITIATED_CRASH);
		DbgPrint("[SETUP] SVM isn't supported on this processor! \n");
		return false;
	}

	if (!IsSvmUnlocked())
	{
		KeBugCheck(MANUALLY_INITIATED_CRASH);
		DbgPrint("[SETUP] SVM operation is locked off in BIOS! \n");
		return false;
	}

	global_hypervisor_data = (GlobalHvData*)ExAllocatePoolZero(NonPagedPool, sizeof(GlobalHvData), 'HvDa');

	BuildNestedPagingTables(&global_hypervisor_data->primary_ncr3, true);
	BuildNestedPagingTables(&global_hypervisor_data->secondary_ncr3, false);
	BuildNestedPagingTables(&global_hypervisor_data->tertiary_cr3, true);

	core_count = KeQueryActiveProcessorCount(0);

	for (int i = 0; i < core_count; ++i)
	{
		KAFFINITY affinity = Utils::ipow(2, i);

		KeSetSystemAffinityThread(affinity);

		DbgPrint("============================================================================= \n");
		DbgPrint("[SETUP] amount of active processors %i \n", core_count);
		DbgPrint("[SETUP] Currently running on core %i \n", i);

		auto reg_context = (CONTEXT*)ExAllocatePoolZero(NonPagedPool, sizeof(CONTEXT), 'Cotx');

		RtlCaptureContext(reg_context);

		if (IsHypervisorPresent(i) == false)
		{
			EnableSvme();

			hv_core_data[i] = (CoreVmcbData*)ExAllocatePoolZero(NonPagedPool, sizeof(CoreVmcbData), 'Vmcb');

			ConfigureProcessor(hv_core_data[i], reg_context);

			SegmentAttribute cs_attrib;

			cs_attrib.as_uint16 = hv_core_data[i]->guest_vmcb.save_state_area.CsAttrib;

			if (IsProcessorReadyForVmrun(&hv_core_data[i]->guest_vmcb, cs_attrib))
			{
				LaunchVm(&hv_core_data[i]->guest_vmcb_physicaladdr);
			}
			else
			{
				DbgPrint("[SETUP] A problem occured!! invalid guest state \n");
				__debugbreak();
			}
		}
		else
		{
			DbgPrint("===================== Hypervisor Successfully Launched rn !! =============================\n \n");
		}
	}

	return true;
}

bool launch_status = true;
void HypervisorEntry()
{
	launch_status = VirtualizeAllProcessors();

	PsTerminateSystemThread(STATUS_SUCCESS);
}

NTSTATUS DriverUnload(PDRIVER_OBJECT DriverObject)
{
	DbgPrint("[AMD-Hypervisor] - Devirtualizing system, Driver unloading!\n");

	return STATUS_SUCCESS;
}

NTSTATUS MapperEntry(PDRIVER_OBJECT DriverObject, PUNICODE_STRING RegistryPath)
{
	DbgPrint("MapperEntry \n");

	HANDLE	thread_handle;
	PsCreateSystemThread(&thread_handle, GENERIC_ALL, NULL, NULL, NULL,
		(PKSTART_ROUTINE)HypervisorEntry, NULL);
	
	LARGE_INTEGER	time;
	time.QuadPart = -10000000;
	KeDelayExecutionThread(KernelMode, TRUE, &time);

	// never do anything directly in mapper entry. 
	// it always causes extremely weird problems. Do everything in newly created system threads.

	return	STATUS_SUCCESS;
}