#include "npt_hook.h"
#include "npthook_safety.h"
#include "logging.h"
#include "disassembly.h"
#include "prepare_vm.h"
#include "vmexit.h"
#include "paging_utils.h"
#include <ntdddisk.h>

Hooks::JmpRipCode ntqvm_hook;

NTSTATUS NTAPI NtQueryVirtualMemory_hook(_In_ HANDLE ProcessHandle, _In_opt_ PVOID BaseAddress, _In_ MEMORY_INFORMATION_CLASS MemoryInformationClass, _Out_writes_bytes_(MemoryInformationLength) PVOID MemoryInformation, _In_ SIZE_T MemoryInformationLength, _Out_opt_ PSIZE_T ReturnLength)
{
	auto status = static_cast<decltype(&NtQueryVirtualMemory)>(ntqvm_hook.original_bytes)(
		ProcessHandle, BaseAddress, MemoryInformationClass, MemoryInformation, MemoryInformationLength, ReturnLength
	);

	return status;
}

Hooks::JmpRipCode ioctl_hk;


NTSTATUS NTAPI NtDeviceIoControlFile_handler(
	_In_ HANDLE FileHandle,
	_In_opt_ HANDLE Event,
	_In_opt_ PIO_APC_ROUTINE ApcRoutine,
	_In_opt_ PVOID ApcContext,
	_Out_ PIO_STATUS_BLOCK IoStatusBlock,
	_In_ ULONG IoControlCode,
	_In_reads_bytes_opt_(InputBufferLength) PVOID InputBuffer,
	_In_ ULONG InputBufferLength,
	_Out_writes_bytes_opt_(OutputBufferLength) PVOID OutputBuffer,
	_In_ ULONG OutputBufferLength
)
{
	/*  original bytes are fucked   */
	auto status = static_cast<decltype(&NtDeviceIoControlFile_handler)>((void*)ioctl_hk.original_bytes)(
		FileHandle,
		Event,
		ApcRoutine,
		ApcContext,
		IoStatusBlock,
		IoControlCode,
		InputBuffer,
		InputBufferLength,
		OutputBuffer,
		OutputBufferLength
	);

	if (IoControlCode == IOCTL_STORAGE_QUERY_PROPERTY)
	{
		DbgPrint("NtDeviceIoControlFile called \n");
	}

	return status;
}

extern "C" void __stdcall LaunchVm(void* vm_launch_params);

bool VirtualizeAllProcessors()
{
	Logger::Get()->Log("[SETUP] test etw log \n");

	int iasdsa = 0;

	DbgPrint("[SETUP] test dbgprint %d \n", iasdsa);

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

	BuildNestedPagingTables(&Hypervisor::Get()->ncr3_dirs[primary], true);
	BuildNestedPagingTables(&Hypervisor::Get()->ncr3_dirs[noexecute], false);
	BuildNestedPagingTables(&Hypervisor::Get()->ncr3_dirs[sandbox], false);

	
	for (int i = 0; i <= NCR3_DIRECTORIES::sandbox; ++i)
	{
		DbgPrint("[SETUP] Hypervisor::Get()->ncr3_dirs[%d] = %p \n", i, Hypervisor::Get()->ncr3_dirs[i]);
	}

	for (int i = 0; i < Hypervisor::Get()->core_count; ++i)
	{
		KAFFINITY affinity = Utils::Exponent(2, i);

		KeSetSystemAffinityThread(affinity);

		DbgPrint("=============================================================== \n");
		DbgPrint("[SETUP] amount of active processors %i \n", Hypervisor::Get()->core_count);
		DbgPrint("[SETUP] Currently running on core %i \n", i);

		auto reg_context = (CONTEXT*)ExAllocatePoolZero(NonPagedPool, sizeof(CONTEXT), 'Cotx');

		RtlCaptureContext(reg_context);

		if (Hypervisor::Get()->IsHypervisorPresent(i) == false)
		{
			EnableSvme();

			auto vcpu_data = Hypervisor::Get()->vcpu_data;

			vcpu_data[i] = (CoreData*)ExAllocatePoolZero(NonPagedPool, sizeof(CoreData), 'Vmcb');

			ConfigureProcessor(vcpu_data[i], reg_context);

			SegmentAttribute cs_attrib;

			cs_attrib.as_uint16 = vcpu_data[i]->guest_vmcb.save_state_area.CsAttrib;

			if (IsProcessorReadyForVmrun(&vcpu_data[i]->guest_vmcb, cs_attrib))
			{
				DbgPrint("address of guest vmcb save state area = %p \n", &vcpu_data[i]->guest_vmcb.save_state_area.Rip);

				LaunchVm(&vcpu_data[i]->guest_vmcb_physicaladdr);
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

	//auto irql = Utils::DisableWP();

	//UNICODE_STRING NtDeviceIoControlFile_name = RTL_CONSTANT_STRING(L"NtDeviceIoControlFile");
	//auto NtDeviceIoControl = (uint8_t*)MmGetSystemRoutineAddress(&NtDeviceIoControlFile_name);

	//ioctl_hk = Hooks::JmpRipCode{ (uintptr_t)NtDeviceIoControl, (uintptr_t)NtDeviceIoControlFile_handler };

	//auto spoofed_page = (uint8_t*)ExAllocatePoolZero(NonPagedPool, 0x1000, 'NIGA');

	//memset(spoofed_page, 0xCC, 0x1000);

	//svm_vmmcall(VMMCALL_ID::remap_page_ncr3_specific, NtDeviceIoControl, spoofed_page, tertiary);

	//DbgPrint("first byte of NtDeviceIoControl: 0x%p \n", *(uint8_t*)NtDeviceIoControl);

	//svm_vmmcall(VMMCALL_ID::remap_page_ncr3_specific, NtDeviceIoControl, spoofed_page, primary);

	//DbgPrint("first byte of NtDeviceIoControl (2): 0x%p \n", *(uint8_t*)NtDeviceIoControl);


	//Utils::EnableWP(irql);

	NPTHooks::PageSynchronizationPatch();
}


int Initialize()
{
	Logger::Get()->Start();
	Disasm::Init();
	//TlbHooks::Init();
	NPTHooks::Init();

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
		&init_thread,
		GENERIC_ALL, NULL, NULL, NULL,
		(PKSTART_ROUTINE)Initialize,
		NULL
	);

	HANDLE thread_handle;

	PsCreateSystemThread(
		&thread_handle, 
		GENERIC_ALL, NULL, NULL, NULL,
		(PKSTART_ROUTINE)VirtualizeAllProcessors,
		NULL
	);

	return STATUS_SUCCESS;
}