#pragma once
#include "hypervisor.h"
#include "npt.h"
#include "paging_utils.h"

namespace Sandbox
{
	struct SandboxPage
	{
		LIST_ENTRY	list_entry;

		PMDL mdl;		/*	mdl used for locking hooked pages	*/
		uint8_t* guest_physical_page;	/*	guest physical address of the sandboxed page	*/
		uint8_t* guest_virtual_page;	/*	guest virtual address of the sandboxed page	*/
		void* hooked_page;				/*	guest virtual address of the hooked page	*/

		PT_ENTRY_64* hookless_npte;		/*	nested PTE of page without hooks			*/
		PT_ENTRY_64* hooked_pte;		/*	guest PTE of the copy page with hooks		*/
		PT_ENTRY_64* guest_pte;			/*	guest PTE of the original page				*/
		int original_nx;				/*	original NX value of the guest PTE			*/

		uintptr_t process_cr3;		/*	process where this hook resides in			*/

		int64_t tag;	/*	identify this hook		*/
		bool active;	/*	is this hook active?	*/

		void Init()
		{
			CR3 cr3;
			cr3.Flags = __readcr3();

			hooked_page = ExAllocatePoolZero(NonPagedPool, PAGE_SIZE, 'HOOK');
			hooked_pte = PageUtils::GetPte(hooked_page, cr3.AddressOfPageDirectory << PAGE_SHIFT);
		}
	};

	extern	void* sandbox_handler;
	extern	int sandbox_page_count;
	extern	SandboxPage* sandbox_page_array;

	uintptr_t EmulateInstruction(VcpuData* vcpu_data, uint8_t* guest_rip, GeneralRegisters* guest_regs, bool is_kernel);

	SandboxPage* IsolatePage(VcpuData* vmcb_data, void* address, int32_t tag = 0);

	SandboxPage* ForEachHook(bool(HookCallback)(SandboxPage* hook_entry, void* data), void* callback_data);

	void ReleasePage(SandboxPage* hook_entry);

	void Init();
};
