#pragma once
#include "hypervisor.h"
#include "npt.h"

namespace NptHooks
{
	struct NptHook
	{
		LIST_ENTRY	list_entry;	

		PMDL mdl;						/*	mdl used for locking hooked pages	*/
	
		uint8_t* guest_physical_page;	/*	guest physical address of the hooked page	*/
		
		void* hooked_page;				/*	guest virtual address of the hooked page	*/

		PT_ENTRY_64* hookless_npte;		/*	nested PTE of page without hooks			*/
		PT_ENTRY_64* hooked_npte;		/*	nested PTE of page without hooks			*/
		PT_ENTRY_64* hooked_pte;		/*	guest PTE of the copy page with hooks		*/
		PT_ENTRY_64* guest_pte;			/*	guest PTE of the original page				*/
		
		int original_nx;				/*	original NX value of the guest PTE			*/
	
		uintptr_t process_cr3;			/*	process where this hook resides in			*/

		void* address;					/*	virtual address of hook			*/

		int32_t ncr3_id;				/*	NCR3 directory that this hook resides in	*/

		NptHook()
		{
			CR3 cr3;
			cr3.Flags = __readcr3();

			hooked_page = ExAllocatePoolZero(NonPagedPool, PAGE_SIZE, 'HOOK');
			hooked_pte = Utils::GetPte(hooked_page, cr3.AddressOfPageDirectory << PAGE_SHIFT);
		}
	};
	

	extern	int	hook_count;
	extern	NptHook* npt_hook_list;

	NptHook* SetNptHook(
		VcpuData* vmcb_data, 
		void* address, 
		uint8_t* patch, 
		size_t patch_len, 
		int32_t shadow_cr3_id
	);

	NptHook* ForEachHook(
		bool(HookCallback)(NptHook* hook_entry, void* data), 
		void* callback_data
	);

	void UnsetHook(
		NptHook* hook_entry
	);

	void Init();
};

#define EASY_NPT_HOOK( shellcode_type, name, function_address ) \
	name = shellcode_type{ (uintptr_t)function_address, (uintptr_t)name##_hook }; \
	svm_vmmcall(VMMCALL_ID::set_npt_hook, function_address, name.hook_code, name.hook_size, NCR3_DIRECTORIES::primary, NULL);
