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
		void* hooked_page;				/*	guest virtual address of the hooked page	*/

		PT_ENTRY_64* hookless_npte;		/*	nested PTE of page without hooks			*/
		PT_ENTRY_64* hooked_pte;		/*	guest PTE of the copy page with hooks		*/

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

	enum SandboxHookId
	{
		readwrite_handler = 0,
		execute_handler = 1,
	};

	extern	void* sandbox_hooks[2];

	extern	int sandbox_page_count;
	extern	SandboxPage* sandbox_page_array;

	void InstructionInstrumentation(VcpuData* vcpu_data, uintptr_t guest_rip, GeneralRegisters* guest_regs, bool is_kernel);

	SandboxPage* AddPageToSandbox(VcpuData* vmcb_data, void* address, int32_t tag = 0);

	SandboxPage* ForEachHook(bool(HookCallback)(SandboxPage* hook_entry, void* data), void* callback_data);

	void ReleasePage(SandboxPage* hook_entry);

	void Init();
};
