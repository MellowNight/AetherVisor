#include "npt_hook.h"
#include "logging.h"

namespace NptHooks
{
	int hook_count;
	NptHook first_npt_hook;

	void Init()
	{
		auto first_hookless_page = ExAllocatePool(NonPagedPool, PAGE_SIZE);

		CR3 cr3;
		cr3.Flags = __readcr3();

		first_npt_hook.hooked_npte = Utils::GetPte(first_hookless_page, cr3.AddressOfPageDirectory << PAGE_SHIFT);
		first_npt_hook.next_hook = (NptHook*)ExAllocatePool(NonPagedPool, sizeof(NptHook));

		auto hook_entry = &first_npt_hook;

		/*	reserve memory for hooks because we can't allocate memory in VM root	*/
		__debugbreak();

		int max_hooks = 20;

		for (int i = 0; i < max_hooks; hook_entry = hook_entry->next_hook, ++i)
		{
			auto hooked_page = ExAllocatePoolZero(NonPagedPool, PAGE_SIZE, 'ENON');

			hook_entry->next_hook = (NptHook*)ExAllocatePool(NonPagedPool, sizeof(NptHook));
			hook_entry->next_hook->hooked_npte = Utils::GetPte(hooked_page, cr3.AddressOfPageDirectory << PAGE_SHIFT);
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

	NptHook* SetNptHook(CoreVmcbData* VpData, void* address, uint8_t* patch, size_t patch_len)
	{
		auto hook_entry = &first_npt_hook;

		for (int i = 0; i < hook_count; hook_entry = hook_entry->next_hook, ++i)
		{
		}

		/*	get the guest pte and physical address of the hooked page	*/

		auto guest_pte = Utils::GetPte(address, VpData->guest_vmcb.save_state_area.Cr3);

		auto physical_page = (uintptr_t)(guest_pte->PageFrameNumber << PAGE_SHIFT);

		if (guest_pte->LargePage)
		{
			PHYSICAL_ADDRESS phys_page;
			phys_page.QuadPart = (uintptr_t)physical_page;

			auto large_page_offset = (uint8_t*)PAGE_ALIGN(address) - MmGetVirtualForPhysical(phys_page);

			physical_page += large_page_offset;
		}

		/*	get the nested pte of the guest physical address	*/

		hook_entry->hook_address = address;
		hook_entry->hookless_npte = Utils::GetPte((void*)physical_page, hypervisor->normal_ncr3);


		/*	get the nested pte of the guest physical address in the 2nd NCR3, and map it to our hook page	*/

		auto hooked_npte = Utils::GetPte((void*)physical_page, hypervisor->noexecute_ncr3);

		hooked_npte->PageFrameNumber = hook_entry->hooked_npte->PageFrameNumber;
		hooked_npte->ExecuteDisable = 0;

		auto hooked_copy = Utils::VirtualAddrFromPfn(hooked_npte->PageFrameNumber);

		auto page_offset = (uintptr_t)address & (PAGE_SIZE - 1);

		
		/*	place our hook in the copied page for the 2nd NCR3	*/

		memcpy(hooked_copy, PAGE_ALIGN(address), PAGE_SIZE);
		memcpy((uint8_t*)hooked_copy + page_offset, patch, patch_len);

		hook_entry->hookless_npte->ExecuteDisable = 1;

		VpData->guest_vmcb.control_area.TlbControl = 3;

		hook_count += 1;

		return hook_entry;
	}
};
