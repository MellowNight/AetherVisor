#include "npt_hook.h"
#include "logging.h"
#include "disassembly.h"
#include "prepare_vm.h"
#include "vmexit.h"
#include "memory_reader.h"

extern "C" void __stdcall LaunchVm(void* vm_launch_params);
extern "C" int __stdcall svm_vmmcall(VMMCALL_ID vmmcall_id, ...);

bool VirtualizeAllProcessors()
{
	if (!IsSvmSupported())
	{
		DbgPrint("[SETUP] SVM isn't supported on this processor! \n");
		return false;
	}

	if (!IsSvmUnlocked())
	{
		DbgPrint("[SETUP] SVM operation is locked off in BIOS! \n");
		return false;
	}
	DbgPrint("[SETUP] asdasdasdasd %p \n", 00000);

	BuildNestedPagingTables(&Hypervisor::Get()->ncr3_dirs[primary], PTEAccess{ true, true, true });
	BuildNestedPagingTables(&Hypervisor::Get()->ncr3_dirs[shadow], PTEAccess{ true, true, false });

	DbgPrint("[SETUP] hypervisor->noexecute_ncr3 %p \n", Hypervisor::Get()->ncr3_dirs[shadow]);
	DbgPrint("[SETUP] hypervisor->normal_ncr3 %p \n", Hypervisor::Get()->ncr3_dirs[primary]);

	Hypervisor::Get()->core_count = KeQueryActiveProcessorCount(0);

	for (int idx = 0; idx < Hypervisor::Get()->core_count; ++idx)
	{
		KAFFINITY affinity = Utils::Exponent(2, idx);

		KeSetSystemAffinityThread(affinity);

		DbgPrint("=============================================================== \n");
		DbgPrint("[SETUP] amount of active processors %i \n", Hypervisor::Get()->core_count);
		DbgPrint("[SETUP] Currently running on core %i \n", idx);

		auto reg_context = (CONTEXT*)ExAllocatePoolZero(NonPagedPool, sizeof(CONTEXT), 'Cotx');

		RtlCaptureContext(reg_context);

		if (Hypervisor::Get()->IsCoreVirtualized(idx) == false)
		{
			EnableSvme();

			auto vcpu_data = Hypervisor::Get()->vcpus;

			vcpu_data[idx] = (VcpuData*)ExAllocatePoolZero(NonPagedPool, sizeof(VcpuData), 'VMCB');

			ConfigureProcessor(vcpu_data[idx], reg_context);

			SegmentAttribute cs_attrib;

			cs_attrib.as_uint16 = vcpu_data[idx]->guest_vmcb.save_state_area.CsAttrib;

			if (IsProcessorReadyForVmrun(&vcpu_data[idx]->guest_vmcb, cs_attrib))
			{
				DbgPrint("address of guest VMCB save state area = %p \n", &vcpu_data[idx]->guest_vmcb.save_state_area.Rip);

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
	//Logger::Get()->Log("first byte of code page is %02x \n", firstbyte);
	//__debugbreak();
}


void Initialize()
{
	Logger::Get()->Start();
	MemoryUtils::Init();
	Disasm::Init();
	//CR3 guest_cr3;
	//guest_cr3.Flags = 0x8be2c000;

	//auto guest_pte = MemoryUtils::GetPte(PAGE_ALIGN(0x0007FFBAA335A40), guest_cr3.AddressOfPageDirectory << PAGE_SHIFT);

	//Logger::Get()->Log("\n ccc guest_pte %p, Physical address %p, MmGetPhysicalAddress %p \n",
	//	*guest_pte, guest_pte->PageFrameNumber << PAGE_SHIFT, MmGetPhysicalAddress(PAGE_ALIGN(0x0007FFBAA335A40)));
	NptHooks::Init();
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
		&init_thread,
		GENERIC_ALL, NULL, NULL, NULL,
		(PKSTART_ROUTINE)Initialize,
		NULL
	);

	Logger::Get()->Log("EntryPoint \n");

	HANDLE thread_handle;

	PsCreateSystemThread(
		&thread_handle, 
		GENERIC_ALL, NULL, NULL, NULL,
		(PKSTART_ROUTINE)VirtualizeAllProcessors,
		NULL
	);

	return STATUS_SUCCESS;
}