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

		PT_ENTRY_64* primary_npte;		/*	nested PTE of page in primary nCR3			*/

		void* guest_physical;		/*	guest physical page address	*/

		int64_t	tag;	/*	identify this sandbox page		*/
		bool active;	/*	is this page actively being sandboxed?	*/

		bool unreadable;	/*	is this page an unreadable page?	*/
	};

	extern SandboxPage*	sandbox_page_array;

	extern int	sandbox_page_count;

	SandboxPage* AddPageToSandbox(
		VcpuData* vmcb_data, 
		void* address, 
		int32_t tag = 0
	);

	void DenyMemoryAccess(VcpuData* vmcb_data, void* address, bool read_only);

	SandboxPage* ForEachHook(
		bool(HookCallback)(SandboxPage* hook_entry, void* data), 
		void* callback_data
	);

	void ReleasePage(
		SandboxPage* hook_entry
	);

	void Init();
};
