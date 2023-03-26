#include "shellcode.h"
#include "portable_executable.h"
#include "vmexit.h"
#include "npt_sandbox.h"
#include "branch_tracer.h"

Hooks::JmpRipCode MmCleanProcessAddressSpace;

char __fastcall MmCleanProcessAddressSpace_hook(__int64 a1, __int64 a2)
{
	/*	unset all NPT hooks for this process	*/

	NptHooks::ForEachHook(
		[](NptHooks::NptHook* hook_entry, void* data) -> auto {

			if (hook_entry->process_cr3 == (uintptr_t)data)
			{
				UnsetHook(hook_entry);
			}

			return false;
		},
		(void*)__readcr3()
	);

	/*	free up all sandboxed pages	*/
	Sandbox::ForEachHook(
		[](Sandbox::SandboxPage* hook_entry, void* data)-> bool {

			if (hook_entry->process_cr3 == (uintptr_t)data)
			{
				Sandbox::ReleasePage(hook_entry);
			}

			return false;
		},
		(void*)__readcr3()
	);

	if (__readcr3() == BranchTracer::process_cr3.Flags)
	{
		Utils::ForEachCore([](void* params) -> auto {

			svm_vmmcall(VMMCALL_ID::stop_branch_trace);
		}, NULL);
	}

	return static_cast<decltype(&MmCleanProcessAddressSpace_hook)>((void*)MmCleanProcessAddressSpace.original_bytes)(a1, a2);
}

void Hypervisor::CleanupOnProcessExit()
{
	/*	Hook NtTerminateProcess and remove NPT hooks inside terminating processes, 
		to prevent physical memory mapping inconsistencies	
	*/

	size_t nt_size = NULL;

	auto ntoskrnl = (uintptr_t)Utils::GetKernelModule(&nt_size, RTL_CONSTANT_STRING(L"ntoskrnl.exe"));

	auto pe_hdr = PE_HEADER(ntoskrnl);

	auto section = (IMAGE_SECTION_HEADER*)(pe_hdr + 1);

	for (int i = 0; i < pe_hdr->FileHeader.NumberOfSections; ++i)
	{
		/*	MmCleanProcessAddressSpace hook to clean up NPT hooks after process exit.
			THIS CAUSES PROBLEMS ON EasyAntiCheat !!! idk why
		*/

		if (!strcmp((char*)section[i].Name, "PAGE"))
		{
			uint8_t* start = section[i].VirtualAddress + (uint8_t*)ntoskrnl;

			uintptr_t clean_process_address_space = Utils::FindPattern((uintptr_t)start, section[i].Misc.VirtualSize, "\xE8\x00\x00\x00\x00\x33\xD2\x48\x8D\x4C\x24\x00\xE8\x00\x00\x00\x00\x4C\x39\xBE", 20, 0x00);

			clean_process_address_space = RELATIVE_ADDR(clean_process_address_space, 1, 5);

			EASY_NPT_HOOK(Hooks::JmpRipCode, MmCleanProcessAddressSpace, clean_process_address_space)
		}
	}
}
