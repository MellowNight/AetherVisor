#include "npt_hook.h"
#include "global.h"
#include "logging.h"

NptHookEntry* GetHookByPhysicalPage(Hypervisor* HvData, UINT64 PagePhysical)
{
	PFN_NUMBER pfn = PagePhysical >> PAGE_SHIFT;

	NptHookEntry* nptHook;

	for (LIST_ENTRY* entry = HvData->hook_list_head; entry != 0; entry = entry->Flink) 
	{
		nptHook = CONTAINING_RECORD(entry, NptHookEntry, npt_hook_list);

		if (nptHook->innocent_npte) 
		{
            if (nptHook->innocent_npte->PageFrameNumber == pfn) 
			{
				return nptHook;
            }
        }
	}

	return 0;
}

NptHookEntry* NewHookEntry()
{
	auto entry = hypervisor->hook_list_head;

	while (MmIsAddressValid(entry->Flink))
	{
		entry = entry->Flink;
	}

	auto new_hook = (NptHookEntry*)ExAllocatePoolZero(NonPagedPool, sizeof(NptHookEntry), 'hook');

	entry->Flink = &new_hook->npt_hook_list;

	return new_hook;
}

/*	execute_only is ONLY possible in usermode, using memory protection key	*/
void SetNptHook(void* address, bool execute_only, uint8_t* patch_bytes, size_t patch_size)
{
	NptHookEntry* hook_entry = NewHookEntry();

	Utils::LockPages(address, IoReadAccess);

	auto hook_physicaladdr = (void*)MmGetPhysicalAddress(address).QuadPart;

	int page_offset = (uintptr_t)hook_physicaladdr & (PAGE_SIZE - 1);

	if (execute_only)
	{
		hook_entry->innocent_npte = Utils::GetPte(hook_physicaladdr, hypervisor->normal_ncr3);
		hook_entry->hooked_npte = Utils::GetPte(hook_physicaladdr, hypervisor->noexecute_ncr3);

		auto page_with_hook = (uintptr_t)ExAllocatePool(NonPagedPool, PAGE_SIZE);

		memcpy((uint8_t*)page_with_hook + page_offset, patch_bytes, patch_size);

		/*	(1) Trigger a page fault when the original page is executed	*/
		hook_entry->innocent_npte->ExecuteDisable = 1;

		/*	(2) map the hook page as executable in the secondary ncr3
			 and switch to second page when (1) happens */
		hook_entry->hooked_npte->ExecuteDisable = 0;
		hook_entry->hooked_npte->PageFrameNumber = Utils::PfnFromVirtualAddr(page_with_hook);
	}
	else
	{
		auto page_with_hook = (uintptr_t)ExAllocatePool(NonPagedPool, PAGE_SIZE);

		hook_entry->innocent_npte = Utils::GetPte(hook_physicaladdr, hypervisor->normal_ncr3);

		/*	copy the innocent npt entry and then point to a different page	*/
		hook_entry->hooked_npte = hook_entry->innocent_npte;
		hook_entry->hooked_npte->PageFrameNumber = Utils::PfnFromVirtualAddr(page_with_hook);

		memcpy((uint8_t*)page_with_hook + page_offset, patch_bytes, patch_size);

		
	}

	Logger::Log(L"innocent_npte pageframe %p \n", hook_entry->innocent_npte->PageFrameNumber);

	hook_entry->execute_only = execute_only;
}