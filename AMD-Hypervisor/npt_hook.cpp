#include "npt_hook.h"

namespace NptHooks{
	NptHook* first_npt_hook = NULL;

	NptHook* FindByHooklessPhysicalPage(uint64_t page_physical)
	{
		PFN_NUMBER pfn = page_physical >> PAGE_SHIFT;

		for (auto hook_entry = first_npt_hook; hook_entry->next_hook != NULL;
			hook_entry = hook_entry->next_hook)
		{
			if (hook_entry->hookless_npte)
			{
				if (hook_entry->hookless_npte->PageFrameNumber == pfn)
				{
					return hook_entry;
				}
			}
		}

		return 0;
	}

	void Init()
	{
		first_npt_hook = NULL;
	}
};
