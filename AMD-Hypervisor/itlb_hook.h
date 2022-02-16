#pragma once
#include "utils.h"
#include "amd_definitions.h"
#include "hypervisor.h"

namespace TlbHooker
{
	// ret_pointer = function pointer to 0xC3 on page, called to trigger an execute access on the target page

	struct SplitTlbHook
	{
		struct SplitTlbHook* next_hook;
		PT_ENTRY_64* hookless_pte;
		PT_ENTRY_64* hooked_pte;
		void(*ret_pointer)();	
	};

	int hook_count;
	SplitTlbHook* first_tlb_hook;

	void Init()
	{
		auto first_hookless_page = (uintptr_t)ExAllocatePool(NonPagedPool, PAGE_SIZE);

		first_tlb_hook = (SplitTlbHook*)ExAllocatePool(NonPagedPool, sizeof(SplitTlbHook));
		first_tlb_hook->hookless_pte->PageFrameNumber = Utils::PfnFromVirtualAddr(first_hookless_page);

		auto hook_entry = first_tlb_hook;

		/*	reserve memory for hooks because we can't allocate memory in VM root	*/

		int max_hooks = 20; 

		for (int i = 0; i < max_hooks; hook_entry = hook_entry->next_hook, ++i)
		{
			auto hookless_page = (uintptr_t)ExAllocatePool(NonPagedPool, PAGE_SIZE);

			hook_entry->next_hook = (SplitTlbHook*)ExAllocatePool(NonPagedPool, sizeof(SplitTlbHook));

			hook_entry->next_hook->hookless_pte->PageFrameNumber = Utils::PfnFromVirtualAddr(hookless_page);
		}

		hook_count = 0;
	}
}

