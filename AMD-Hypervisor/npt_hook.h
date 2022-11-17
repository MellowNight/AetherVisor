#pragma once
#include "hypervisor.h"
#include "npt.h"
#include "paging_utils.h"

namespace NptHooks
{
	struct NptHook
	{
		LIST_ENTRY	list_entry;	

		PMDL mdl;		/*	mdl used for locking hooked pages	*/
		uint8_t* guest_physical_page;	/*	guest physical address of the hooked page	*/
		void* hooked_page;				/*	guest virtual address of the hooked page	*/

		PT_ENTRY_64* hookless_npte;		/*	nested PTE of page without hooks			*/
		PT_ENTRY_64* hooked_pte;		/*	guest PTE of the copy page with hooks		*/
		PT_ENTRY_64* guest_pte;			/*	guest PTE of the original page				*/
		int original_nx;				/*	original NX value of the guest PTE			*/
	
		uintptr_t process_cr3;		/*	process where this hook resides in			*/

		int32_t tag;	/*	identify this hook		*/
		bool active;	/*	is this hook active?	*/

		uintptr_t noexecute_ncr3;

		void Init()
		{
			CR3 cr3;
			cr3.Flags = __readcr3();

			hooked_page = ExAllocatePoolZero(NonPagedPool, PAGE_SIZE, 'HOOK');
			hooked_pte = PageUtils::GetPte(hooked_page, cr3.AddressOfPageDirectory << PAGE_SHIFT);
		}
	};
	

	extern	int hook_count;
	extern	NptHook* npt_hook_array;

	NptHook* SetNptHook(CoreData* vmcb_data, void* address, uint8_t* patch, size_t patch_len, int32_t noexecute_cr3_id, int32_t tag = 0);
	NptHook* ForEachHook(bool(HookCallback)(NptHook* hook_entry, void* data), void* callback_data);

	void UnsetHook(NptHook* hook_entry);

	void Init();
};
