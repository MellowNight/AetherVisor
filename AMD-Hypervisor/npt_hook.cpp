#include "npt_hook.h"

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

		int max_hooks = 20;

		for (int i = 0; i < max_hooks; hook_entry = hook_entry->next_hook, ++i)
		{
			auto hookless_page = ExAllocatePool(NonPagedPool, PAGE_SIZE);

			hook_entry->next_hook = (NptHook*)ExAllocatePool(NonPagedPool, sizeof(NptHook));
			hook_entry->next_hook->hooked_npte = Utils::GetPte(hookless_page, cr3.AddressOfPageDirectory << PAGE_SHIFT);
		}

		hook_count = 0;
	}

	NptHook* SetNptHook(void* address, uint8_t* patch, size_t patch_len)
	{
		auto hook_entry = &first_tlb_hook;

		for (int i = 0; i < hook_count; hook_entry = hook_entry->next_hook, ++i)
		{
		}

		Utils::LockPages(address, IoReadAccess);

		auto physical_addr = (void*)MmGetPhysicalAddress(address).QuadPart;

		hook_entry->hookless_npte = Utils::GetPte(physical_addr, hypervisor->normal_ncr3);
		hook_entry->hooked_npte = Utils::GetPte(physical_addr, hypervisor->SecondaryNCr3);

		auto hooked_copy = ExAllocatePool(NonPagedPool, PAGE_SIZE);

		memcpy(hooked_copy, address, PAGE_SIZE);

		hook_entry->hooked_npte->ExecuteDisable = 0;
		hook_entry->hooked_npte->PageFrameNumber = Utils::PfnFromVirtualAddr(hooked_copy);

		auto page_offset = (uintptr_t)address & (PAGE_SIZE - 1);

	}
};
