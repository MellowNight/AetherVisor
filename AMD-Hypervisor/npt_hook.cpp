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
			auto hookless_copy_page = ExAllocatePoolZero(NonPagedPool, PAGE_SIZE, 'ENON');

			hook_entry->hookless_copy_page = (void*)(Utils::GetPte(
				hookless_copy_page, 
				cr3.AddressOfPageDirectory << PAGE_SHIFT
			)->PageFrameNumber << PAGE_SHIFT);

			Logger::Log("hooked_page %p \n", hooked_page);
			Logger::Log("hookless_copy_page %p \n", hook_entry->hookless_copy_page);

			hook_entry->hooked_npte = Utils::GetPte(hooked_page, cr3.AddressOfPageDirectory << PAGE_SHIFT);

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

	NptHook* SetNptHook(CoreVmcbData* VpData, void* address, uint8_t* patch, size_t patch_len)
	{
		auto hook_entry = &first_npt_hook;

		for (int i = 0; i < hook_count; hook_entry = hook_entry->next_hook, ++i)
		{
		}

		Utils::LockPages(address, IoReadAccess);

		/*	get the guest pte and physical address of the hooked page	*/

		auto guest_cr3 = VpData->guest_vmcb.save_state_area.Cr3;
		auto guest_pte = Utils::GetPte(address, guest_cr3);

		auto guest_physical_page = (uintptr_t)(guest_pte->PageFrameNumber << PAGE_SHIFT);

		PHYSICAL_ADDRESS phys_page;
		phys_page.QuadPart = (uintptr_t)guest_physical_page;

		auto page_offset = (uintptr_t)address & (PAGE_SIZE - 1);

		hook_entry->hook_guest_context.Flags = guest_cr3;

		if (guest_pte->LargePage)
		{
			Logger::Log("\n PAGE_ALIGN(address) %p \n", PAGE_ALIGN(address));
			Logger::Log("\n MmGetVirtualForPhysical(phys_page) %p \n", MmGetVirtualForPhysical(phys_page));

			auto large_page_offset = (uint8_t*)PAGE_ALIGN(address) - MmGetVirtualForPhysical(phys_page);

			guest_physical_page += large_page_offset;
		}

		Logger::Log("\n physical_page %p \n", guest_physical_page);

		/*	get the nested pte of the guest physical address	*/

		hook_entry->guest_phys_addr = (uint8_t*)guest_physical_page + page_offset;
		hook_entry->hookless_npte = Utils::GetPte((void*)guest_physical_page, hypervisor->normal_ncr3);

		/*	get the nested pte of the guest physical address in the 2nd NCR3, and map it to our hook page	*/

		auto hooked_npte = Utils::GetPte((void*)guest_physical_page, hypervisor->noexecute_ncr3);

		hooked_npte->PageFrameNumber = hook_entry->hooked_npte->PageFrameNumber;
		hooked_npte->ExecuteDisable = 0;

		auto hooked_copy = Utils::VirtualAddrFromPfn(hooked_npte->PageFrameNumber);

		Logger::Log("hooked_copy %p \n", hooked_copy);
		
		/*	place our hook in the copied page for the 2nd NCR3	*/
		Logger::Log("the address is %p \n", address);

		memcpy(
			hooked_copy, 
			MemoryUtils::MapVirtualMem(PAGE_ALIGN(address), hook_entry->hook_guest_context), 
			PAGE_SIZE
		);
		Logger::Log("Utils::VirtualAddrFromPfn((uintptr_t)hook_entry->hookless_copy_page >> PAGE_SHIFT) %p hooked_copy %p \n", 
			Utils::VirtualAddrFromPfn((uintptr_t)hook_entry->hookless_copy_page >> PAGE_SHIFT),
			hooked_copy);

		memcpy(
			Utils::VirtualAddrFromPfn((uintptr_t)hook_entry->hookless_copy_page >> PAGE_SHIFT),
			hooked_copy,
			PAGE_SIZE
		);

		memcpy(
			(uint8_t*)hooked_copy + page_offset, 
			MemoryUtils::MapVirtualMem(patch, hook_entry->hook_guest_context), 
			patch_len
		);

		hook_entry->hookless_npte->ExecuteDisable = 1;

		VpData->guest_vmcb.control_area.TlbControl = 3;

		hook_count += 1;

		return hook_entry;
	}
};
