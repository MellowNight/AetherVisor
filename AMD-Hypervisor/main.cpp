#include "itlb_hook.h"
#include "npt_hook.h"
#include "logging.h"
#include "disassembly.h"
#include "prepare_vm.h"
#include "vmexit.h"
#include "memory_reader.h"
#include "mem_prot_key.h"

extern "C" void __stdcall LaunchVm(void* vm_launch_params);
extern "C" int __stdcall svm_vmmcall(VMMCALL_ID vmmcall_id, ...);

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
	BuildNestedPagingTables(&hypervisor->noexecute_ncr3, true);

	DbgPrint("[SETUP] hypervisor->noexecute_ncr3 %p \n", hypervisor->noexecute_ncr3); 
	DbgPrint("[SETUP] hypervisor->normal_ncr3 %p \n", hypervisor->normal_ncr3);

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


	/*	experiment with TLB spliting	*/
	//LARGE_INTEGER delay = { 30000000 };	// 3 seconds
	//KeDelayExecutionThread(KernelMode, FALSE, &delay);

	//auto code_page = ExAllocatePool(NonPagedPool, PAGE_SIZE);
	//memset(code_page, 0xCC, PAGE_SIZE);
	//*(char*)code_page = 0xC3;

	//TlbHooks::SetTlbHook(code_page, (uint8_t*)"\xCC\xC3", 2);

	//__debugbreak();
	//static_cast<void(*)()>(code_page)();

	//unsigned char firstbyte = *(unsigned char*)code_page & 0xFF;
	//Logger::Log("first byte of code page is %02x \n", firstbyte);
	//__debugbreak();
}


void Initialize()
{
	MemoryUtils::Init();
	Logger::Start();
	Disasm::Init();
	TlbHooks::Init();
	NptHooks::Init();
	MpkHooks::Init();
}

NTSTATUS DriverUnload(PDRIVER_OBJECT DriverObject)
{
	Logger::Log("[AMD-Hypervisor] - Devirtualizing system, Driver unloading!\n");

	return STATUS_SUCCESS;
}

NTSTATUS EntryPoint(PDRIVER_OBJECT DriverObject, PUNICODE_STRING RegistryPath)
{
	HANDLE init_thread;

	PsCreateSystemThread(
		&init_thread,
		GENERIC_ALL, NULL, NULL, NULL,
		(PKSTART_ROUTINE)Initialize,
		NULL
	);

	Logger::Log("EntryPoint \n");

	HANDLE thread_handle;

	PsCreateSystemThread(
		&thread_handle, 
		GENERIC_ALL, NULL, NULL, NULL,
		(PKSTART_ROUTINE)VirtualizeAllProcessors,
		NULL
	);

	return STATUS_SUCCESS;
}