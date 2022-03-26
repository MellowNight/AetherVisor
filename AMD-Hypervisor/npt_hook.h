#pragma once
#include "hypervisor.h"
#include "npt.h"

namespace NptHooks
{
	struct NptHook
	{
		struct NptHook* next_hook;
		void* guest_phys_addr;
		CR3 hook_guest_context;
		PT_ENTRY_64* hookless_npte;
		PT_ENTRY_64* hooked_npte;
		uint8_t* hookless_copy_page;
	};
	
	extern	int hook_count;
	extern	NptHook first_npt_hook;

	NptHook* SetNptHook(CoreVmcbData* VpData, void* address, uint8_t* patch, size_t patch_len);
	NptHook* ForEachHook(bool(HookCallback)(NptHook* hook_entry, void* data), void* callback_data);

	void Init();
};
