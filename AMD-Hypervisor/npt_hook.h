#pragma once
#include "hypervisor.h"
#include "npt.h"

/*	To do: use singleton	*/

namespace NptHooks
{
	struct NptHook
	{
		struct NptHook* next_hook;

		uint8_t* guest_phys_addr;

		PT_ENTRY_64* hookless_npte;
		PT_ENTRY_64* hooked_pte;

		PT_ENTRY_64* copy_pte;
		PT_ENTRY_64* original_pte;

		uint64_t original_pfn;

		uintptr_t process_cr3;

		int32_t tag;
	};
	
	extern	int hook_count;
	extern	NptHook first_npt_hook;

	NptHook* SetNptHook(CoreData* VpData, void* address, uint8_t* patch, size_t patch_len, int32_t tag = 0);
	NptHook* ForEachHook(bool(HookCallback)(NptHook* hook_entry, void* data), void* callback_data);
	void PageSynchronizationPatch();

	void RemoveHook(int32_t tag);

	void Init();
};
