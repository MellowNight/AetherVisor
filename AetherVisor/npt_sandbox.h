#pragma once
#include "hypervisor.h"
#include "npt.h"

namespace Sandbox
{
	struct SandboxPage
	{
		uintptr_t process_cr3;

		uint32_t id;

		PMDL mdl;		/*	mdl used for locking hooked pages	*/

		PT_ENTRY_64* primary_npte;		/*	nested PTE of page in primary nCR3			*/

		void* guest_physical;		/*	guest physical page address	*/

		bool denied_access;	/*	is this page an unreadable page?	*/
	};

	extern SandboxPage*	sandbox_page_array;

	extern int	sandbox_page_count;

	extern uintptr_t branch_exclusion_range_base;

	SandboxPage* AddPageToSandbox(
		VcpuData* vmcb_data, 
		void* address, 
		int32_t tag = 0
	);

	void DenyMemoryAccess(
		VcpuData* vmcb_data, 
		void* address, 
		bool allow_reads
	);

	SandboxPage* ForEachHook(
		bool(HookCallback)(SandboxPage* hook_entry, void* data), 
		void* callback_data
	);

	void ReleasePage(
		SandboxPage* hook_entry
	);

	void Init();
};
