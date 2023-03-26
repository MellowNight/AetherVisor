#include "npt_hook.h"
#include "logging.h"
#include "disassembly.h"
#include "prepare_vm.h"
#include "vmexit.h"
#include "npt_sandbox.h"

extern "C" void __stdcall LaunchVm(void* vm_launch_params);

bool VirtualizeAllProcessors()
{
	if (!IsSvmSupported())
	{
		Logger::Get()->Log("[SETUP] SVM isn't supported on this processor! \n");
		return false;
	}

	if (!IsSvmUnlocked())
	{
		Logger::Get()->Log("[SETUP] SVM operation is locked off in BIOS! \n");
		return false;
	}

	BuildNestedPagingTables(&Hypervisor::Get()->ncr3_dirs[primary], PTEAccess{ true, true, true });
	BuildNestedPagingTables(&Hypervisor::Get()->ncr3_dirs[shadow], PTEAccess{ true, true, false });
	BuildNestedPagingTables(&Hypervisor::Get()->ncr3_dirs[sandbox], PTEAccess{ true, true, false });
	BuildNestedPagingTables(&Hypervisor::Get()->ncr3_dirs[sandbox_single_step], PTEAccess{ true, true, true });

	Hypervisor::Get()->core_count = KeQueryActiveProcessorCount(0);

	Utils::ForEachCore([](void* params) -> void {

		PROCESSOR_NUMBER processor_num;

		KeGetCurrentProcessorNumberEx(&processor_num);

		auto idx = KeGetProcessorIndexFromNumber(&processor_num);

		KAFFINITY affinity = Utils::Exponent(2, idx);

		KeSetSystemAffinityThread(affinity);

		DbgPrint("=============================================================== \n");
		DbgPrint("[SETUP] amount of active processors %i \n", Hypervisor::Get()->core_count);
		DbgPrint("[SETUP] Currently running on core %i \n", idx);

		auto register_ctx = (CONTEXT*)ExAllocatePoolZero(NonPagedPool, sizeof(CONTEXT), 'Cotx');

		RtlCaptureContext(register_ctx);

		if (Hypervisor::Get()->IsCoreVirtualized(idx) == false)
		{
			EnableSvme();

			auto vcpu_data = Hypervisor::Get()->vcpus;

			vcpu_data[idx] = (VcpuData*)ExAllocatePoolZero(NonPagedPool, sizeof(VcpuData), 'Vmcb');

			vcpu_data[idx]->ConfigureProcessor(register_ctx);

			auto cs_attrib = vcpu_data[idx]->guest_vmcb.save_state_area.cs_attrib;

			if (IsCoreReadyForVmrun(&vcpu_data[idx]->guest_vmcb, cs_attrib))
			{
				DbgPrint("address of guest vmcb save state area = %p \n", &vcpu_data[idx]->guest_vmcb.save_state_area.rip);

				LaunchVm(&vcpu_data[idx]->guest_vmcb_physicaladdr);
			}
			else
			{
				Logger::Get()->Log("[SETUP] A problem occured!! invalid guest state \n");
				__debugbreak();
			}
		}
		else
		{
			DbgPrint("============== Hypervisor Successfully Launched rn !! ===============\n \n");
		}
	}, NULL);

	size_t nt_size;
	UNICODE_STRING ntos_name = RTL_CONSTANT_STRING(L"ntoskrnl.exe");

	auto nt_base = (uint8_t*)Utils::GetKernelModule(&nt_size, ntos_name);

	bool (*KeRelaxTimingConstraints)(int one_or_zero) = static_cast<decltype(KeRelaxTimingConstraints)>((void*)(nt_base + 0x50F408));

	KeRelaxTimingConstraints(1);

	//	unlock MDLs and remove NPT hooks in terminating processes

	Hypervisor::Get()->CleanupOnProcessExit();
}


int Initialize()
{
	Logger::Get()->Start();

	Disasm::Init();

	Sandbox::Init();
	NptHooks::Init();

	return 0;
}

NTSTATUS DriverUnload(PDRIVER_OBJECT DriverObject)
{
	Logger::Get()->Log("[AMD-Hypervisor] - Devirtualizing system, Driver unloading!\n");

	return STATUS_SUCCESS;
}

NTSTATUS EntryPoint(PDRIVER_OBJECT DriverObject, PUNICODE_STRING RegistryPath)
{
	HANDLE init_thread;

	PsCreateSystemThread(
		&init_thread, GENERIC_ALL, NULL, NULL, NULL, (PKSTART_ROUTINE)Initialize, NULL);

	HANDLE hv_startup_thread;

	PsCreateSystemThread(
		&hv_startup_thread, GENERIC_ALL, NULL, NULL, NULL, (PKSTART_ROUTINE)VirtualizeAllProcessors, NULL);

	return STATUS_SUCCESS;
}