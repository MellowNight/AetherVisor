#include "npthook_safety.h"
#include "portable_executable.h"
#include "vmexit.h"

namespace NptHooks
{
	Hooks::JmpRipCode MmCleanProcessAddressSpace;

	char __fastcall MmCleanProcessAddressSpace_hook(__int64 a1, __int64 a2)
	{
		/*	unset all NPT hooks for this process	*/

		ForEachHook(
			[](NptHook* hook_entry, void* data)-> bool {

				if (hook_entry->process_cr3 == (uintptr_t)data)
				{
					UnsetHook(hook_entry);
				}

				return false;
			},
			(void*)__readcr3()
		);

		return static_cast<decltype(&MmCleanProcessAddressSpace_hook)>(MmCleanProcessAddressSpace.original_bytes)(a1, a2);
	}

	void CleanupOnProcessExit()
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
			/*	MmCleanProcessAddressSpace hook to clean up NPT hooks after process exit	*/

			if (!strcmp((char*)section[i].Name, "PAGE"))
			{
				uint8_t* start = section[i].VirtualAddress + (uint8_t*)ntoskrnl;

				uintptr_t clean_process_address_space = Utils::FindPattern((uintptr_t)start, section[i].Misc.VirtualSize, "\xE8\x00\x00\x00\x00\x33\xD2\x48\x8D\x4C\x24\x00\xE8\x00\x00\x00\x00\x4C\x39\xBE", 20, 0x00);

				clean_process_address_space = RELATIVE_ADDR(clean_process_address_space, 1, 5);
//				name = shellcode_type{ (uintptr_t)function_address, (uintptr_t)name##_hook }; 
				svm_vmmcall(VMMCALL_ID::set_npt_hook, ntoskrnl + 0x4FCE20, "\xCC", 1, NCR3_DIRECTORIES::primary, NULL);

				// EASY_NPT_HOOK(Hooks::JmpRipCode, MmCleanProcessAddressSpace, clean_process_address_space)
			}
		}
	}
};
