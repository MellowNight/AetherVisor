#include "npt_hook.h"
#include "logging.h"
#include "paging_utils.h"
#include "disassembly.h"
#include "portable_executable.h"
#include "vmexit.h"
#include "utils.h"
#include "shellcode.h"

namespace NptHooks
{
	int hook_count;
	NptHook first_npt_hook;


	Hooks::JmpRipCode hk_NtTerminateProcess;

	__int64 __fastcall NtTerminateProcess_hook(__int64 a1, __int64 a2)
	{
		/*	unset all NPT hooks for this process	*/
		RemoveHook(__readcr3());

		return static_cast<decltype(&NtTerminateProcess_hook)>(hk_NtTerminateProcess.original_bytes)(a1, a2);
	}


	void PageSynchronizationPatch()
	{
		/*	place a callback on NtTerminateProcess to remove npt hooks inside terminating processes, to prevent PFN check bsods	*/

		uintptr_t terminate_process;

		size_t nt_size = NULL;
		auto ntoskrnl = Utils::GetDriverBaseAddress(&nt_size, RTL_CONSTANT_STRING(L"ntoskrnl.exe"));

		auto pe_hdr = PeHeader(ntoskrnl);

		auto section = (IMAGE_SECTION_HEADER*)(pe_hdr + 1);

		for (int i = 0; i < pe_hdr->FileHeader.NumberOfSections; ++i)
		{
			if (!strcmp((char*)section[i].Name, "PAGE"))
			{
				uint8_t* start = section[i].VirtualAddress + (uint8_t*)ntoskrnl;

				terminate_process = Utils::FindPattern((uintptr_t)start, section[i].Misc.VirtualSize, "\x4C\x8B\xDC\x49\x89\x5B\x10\x49\x89\x6B\x18\x57", 12, 0x00); // MmGetSystemRoutineAddress(&terminate_process_name);
			}
		}

		DbgPrint("terminate_process %p \n", terminate_process);

		hk_NtTerminateProcess = Hooks::JmpRipCode{ (uintptr_t)terminate_process, (uintptr_t)NtTerminateProcess_hook };

		Utils::ForEachCore([](void* params) -> void {

			auto jmp_rip_code = (Hooks::JmpRipCode*)params;

			LARGE_INTEGER length_tag;
			length_tag.LowPart = NULL;
			length_tag.HighPart = jmp_rip_code->hook_size;

			svm_vmmcall(VMMCALL_ID::set_npt_hook, jmp_rip_code->hook_addr, jmp_rip_code->hook_code, length_tag.QuadPart);

		}, &hk_NtTerminateProcess);
	}

	NptHook* ForEachHook(bool(HookCallback)(NptHook* hook_entry, void* data), void* callback_data)
	{
		auto hook_entry = &first_npt_hook;

		for (int i = 0; i < hook_count; hook_entry = hook_entry->next_hook, ++i)
		{
			if (HookCallback(hook_entry, callback_data))
			{
				return hook_entry;
			}
		}
		return 0;
	}

	void Init()
	{
		auto first_hookless_page = ExAllocatePoolZero(NonPagedPool, PAGE_SIZE, 'ENON');

		CR3 cr3;
		cr3.Flags = __readcr3();

		/*	reserve memory for hooks because we can't allocate memory in VM root	*/

		int max_hooks = 500;
		
		NptHook* hook_entry = &first_npt_hook;

		for (int i = 0; i < max_hooks; hook_entry = hook_entry->next_hook, ++i)
		{
			auto hooked_page = ExAllocatePoolZero(NonPagedPool, PAGE_SIZE, 'ENON');

			hook_entry->hooked_pte = PageUtils::GetPte(hooked_page, cr3.AddressOfPageDirectory << PAGE_SHIFT);

			auto copy_page = ExAllocatePoolZero(NonPagedPool, PAGE_SIZE, 'ENON');

			hook_entry->copy_pte = PageUtils::GetPte(copy_page, cr3.AddressOfPageDirectory << PAGE_SHIFT);
			
			hook_entry->next_hook = (NptHook*)ExAllocatePool(NonPagedPool, sizeof(NptHook));
		}
		hook_count = 0;
	}

	void RemoveHook(int32_t tag)
	{
		ForEachHook(
			[](NptHook* hook_entry, void* data)-> bool {

				if ((int32_t)data == hook_entry->tag)
				{
					/*	TO DO: restore guest PFN, free and unlink hook from list	*/

					hook_entry->tag = 0;
					hook_entry->hookless_npte->ExecuteDisable = 0;
					hook_entry->original_pte->PageFrameNumber = hook_entry->original_pfn;

					return true;
				}

			}, (void*)tag
		);
	}

#pragma optimize( "", off )
	void RemoveHook(uintptr_t current_cr3)
	{
		ForEachHook(
			[](NptHook* hook_entry, void* data)-> bool {

				DbgPrint("hook entry cr3 %p \n", hook_entry->process_cr3);
				DbgPrint("current cr3 %p \n", __readcr3());

				if ((uintptr_t)data == hook_entry->process_cr3)
				{
					DbgPrint("hook_entry found %p \n", hook_entry);

					/*	TO DO: restore guest PFN, free and unlink hook from list	*/

					hook_entry->tag = 0;
					hook_entry->hookless_npte->ExecuteDisable = 0;
					hook_entry->original_pte->PageFrameNumber = hook_entry->original_pfn;

					return true;
				}

			}, (void*)current_cr3
		);
	}
#pragma optimize( "", on )

	NptHook* SetNptHook(CoreData* vmcb_data, void* address, uint8_t* patch, size_t patch_len, int32_t tag)
	{
		auto hook_entry = &first_npt_hook;

		for (int i = 0; i < hook_count; hook_entry = hook_entry->next_hook, ++i)
		{
		}

		hook_entry->tag = tag;
		hook_entry->process_cr3 = vmcb_data->guest_vmcb.save_state_area.Cr3;

		auto vmroot_cr3 = __readcr3();

		__writecr3(vmcb_data->guest_vmcb.save_state_area.Cr3);

		/*	Sometimes we want to place a hook in a globally mapped DLL like user32.dll, ntdll.dll, etc. but we only want our hook to exist in one context.
			set the guest pte to point to a new copy page, to prevent the hook from being globally mapped.	*/
		if (vmroot_cr3 != vmcb_data->guest_vmcb.save_state_area.Cr3)
		{
			auto guest_pte = PageUtils::GetPte((void*)address, vmcb_data->guest_vmcb.save_state_area.Cr3);

			hook_entry->original_pte = guest_pte;
			hook_entry->original_pfn = guest_pte->PageFrameNumber;

			guest_pte->PageFrameNumber = hook_entry->copy_pte->PageFrameNumber;

			memcpy(PageUtils::VirtualAddrFromPfn(hook_entry->copy_pte->PageFrameNumber), PAGE_ALIGN(address), PAGE_SIZE);
		}

		/*	get the guest pte and physical address of the hooked page	*/

		auto physical_page = PAGE_ALIGN(MmGetPhysicalAddress(address).QuadPart);

		hook_entry->guest_phys_addr = (uint8_t*)physical_page;

		/*	get the nested pte of the guest physical address	*/

		hook_entry->hookless_npte = PageUtils::GetPte((void*)physical_page, Hypervisor::Get()->normal_ncr3);
		hook_entry->hookless_npte->ExecuteDisable = 1;


		/*	get the nested pte of the guest physical address in the 2nd NCR3, and map it to our hook page	*/

		auto hooked_npte = PageUtils::GetPte((void*)physical_page, Hypervisor::Get()->noexecute_ncr3);

		hooked_npte->PageFrameNumber = hook_entry->hooked_pte->PageFrameNumber;
		hooked_npte->ExecuteDisable = 0;
		
		auto hooked_copy = PageUtils::VirtualAddrFromPfn(hooked_npte->PageFrameNumber);

		auto page_offset = (uintptr_t)address & (PAGE_SIZE - 1);


		/*	place our hook in the copied page for the 2nd NCR3	*/

		memcpy(hooked_copy, PAGE_ALIGN(address), PAGE_SIZE);
		memcpy((uint8_t*)hooked_copy + page_offset, patch, patch_len);

		/*	SetNptHook epilogue	*/

		vmcb_data->guest_vmcb.control_area.TlbControl = 3;

		__writecr3(vmroot_cr3);

		hook_count += 1;

		return hook_entry;
	}

};
