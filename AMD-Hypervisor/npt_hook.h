#pragma once
#include "hypervisor.h"
#include "npt.h"
#include "paging_utils.h"

namespace NptHooks
{
	struct NptHook
	{
		LIST_ENTRY	list_entry;	

		uint8_t* guest_phys_addr;		/*	guest physical address of the hooked page	*/
		void* hooked_page;				/*	guest virtual address of the hooked page	*/

		PT_ENTRY_64* hookless_npte;		/*	nested PTE of page without hooks	*/
		PT_ENTRY_64* hooked_pte;		/*	guest PTE of page with hooks		*/

		PT_ENTRY_64* copy_pte;			/*	guest PTE of the copy page	(for globally mapped DLL hooks)	*/
		void* copy_page;

		uintptr_t process_cr3;			/*	process where this hook resides in	*/

		int32_t tag;		/*	identify this hook		*/
		bool active;		/*	is this hook active?	*/

		void Init()
		{
			CR3 cr3;
			cr3.Flags = __readcr3();

			hooked_page = ExAllocatePoolZero(NonPagedPool, PAGE_SIZE, 'HOOK');
			hooked_pte = PageUtils::GetPte(hooked_page, cr3.AddressOfPageDirectory << PAGE_SHIFT);

			copy_page = ExAllocatePoolZero(NonPagedPool, PAGE_SIZE, 'HOOK');
			copy_pte = PageUtils::GetPte(copy_page, cr3.AddressOfPageDirectory << PAGE_SHIFT);
		}
	};
	

	extern	int hook_count;
	extern	NptHook first_npt_hook;

	NptHook* SetNptHook(CoreData* VpData, void* address, uint8_t* patch, size_t patch_len, int32_t tag = 0);
	NptHook* ForEachHook(bool(HookCallback)(NptHook* hook_entry, void* data), void* callback_data);

	void UnsetHook(NptHook* hook_entry);
	void ReserveHookEntry();

	void Init();
};
