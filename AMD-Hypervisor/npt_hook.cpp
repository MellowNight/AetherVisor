#include "npt_hook.h"
#include "logging.h"
#include "memory_reader.h"

namespace NptHooks
{
	int hook_count;
	NptHook first_npt_hook;

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

	NptHook* SetNptHook(CoreData* vmcb_data, void* address, uint8_t* patch, size_t patch_len, int32_t tag)
	{
		auto hook_entry = &first_npt_hook;

		for (int i = 0; i < hook_count; hook_entry = hook_entry->next_hook, ++i)
		{
		}

		hook_entry->tag = tag;

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

			memcpy(Utils::VirtualAddrFromPfn(hook_entry->copy_pte->PageFrameNumber), PAGE_ALIGN(address), PAGE_SIZE);
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
		
		auto hooked_copy = Utils::VirtualAddrFromPfn(hooked_npte->PageFrameNumber);

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
