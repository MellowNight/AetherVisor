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

		int max_hooks = 20;
		
		NptHook* hook_entry = &first_npt_hook;

		for (int i = 0; i < max_hooks; hook_entry = hook_entry->next_hook, ++i)
		{
			auto hooked_page = ExAllocatePoolZero(NonPagedPool, PAGE_SIZE, 'ENON');

			hook_entry->hooked_pte = MemoryUtils::GetPte(hooked_page, cr3.AddressOfPageDirectory << PAGE_SHIFT);

			auto copy_page = ExAllocatePoolZero(NonPagedPool, PAGE_SIZE, 'ENON');

			hook_entry->copy_pte = MemoryUtils::GetPte(copy_page, cr3.AddressOfPageDirectory << PAGE_SHIFT);
			
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

#define LARGE_PAGE_SIZE 0x200000
#define LARGE_PAGE_ALIGN(Va) ((uint8_t*)((ULONG_PTR)(Va) & ~(LARGE_PAGE_SIZE - 1)))


	NptHook* SetNptHook(CoreVmcbData* VpData, void* address, uint8_t* patch, size_t patch_len)
	{
		auto hook_entry = &first_npt_hook;

		for (int i = 0; i < hook_count; hook_entry = hook_entry->next_hook, ++i)
		{
		}

		auto vmroot_cr3 = __readcr3();

		auto guest_cr3 = VpData->guest_vmcb.save_state_area.Cr3;

		__writecr3(guest_cr3);

		/*	Sometimes we want to place a hook in a globally mapped DLL like user32.dll, ntdll.dll, etc. but we only want our hook to exist in one context.
			set the guest pte to point to a new copy page, to prevent the hook from being globally mapped.	*/
		if (vmroot_cr3 != guest_cr3)
		{
			auto guest_pte = MemoryUtils::GetPte((void*)address, guest_cr3);

			guest_pte->PageFrameNumber = hook_entry->copy_pte->PageFrameNumber;

			memcpy(Utils::VirtualAddrFromPfn(hook_entry->copy_pte->PageFrameNumber), PAGE_ALIGN(address), PAGE_SIZE);
		}

		/*	get the guest pte and physical address of the hooked page	*/

		auto physical_page = PAGE_ALIGN(MmGetPhysicalAddress(address).QuadPart);

		hook_entry->guest_phys_addr = (uint8_t*)physical_page;
		

		/*	get the nested pte of the guest physical address	*/

		hook_entry->hookless_npte = MemoryUtils::GetPte((void*)physical_page, hypervisor->normal_ncr3);
		hook_entry->hookless_npte->ExecuteDisable = 1;


		/*	get the nested pte of the guest physical address in the 2nd NCR3, and map it to our hook page	*/

		auto hooked_npte = MemoryUtils::GetPte((void*)physical_page, hypervisor->noexecute_ncr3);

		hooked_npte->PageFrameNumber = hook_entry->hooked_pte->PageFrameNumber;
		hooked_npte->ExecuteDisable = 0;
		
		auto hooked_copy = Utils::VirtualAddrFromPfn(hooked_npte->PageFrameNumber);

		auto page_offset = (uintptr_t)address & (PAGE_SIZE - 1);


		/*	place our hook in the copied page for the 2nd NCR3	*/

		memcpy(hooked_copy, PAGE_ALIGN(address), PAGE_SIZE);
		memcpy((uint8_t*)hooked_copy + page_offset, patch, patch_len);


		/*	SetNptHook epilogue	*/

		VpData->guest_vmcb.control_area.TlbControl = 3;

		__writecr3(vmroot_cr3);

		hook_count += 1;

		return hook_entry;
	}
};
