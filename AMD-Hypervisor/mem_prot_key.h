#pragma once
#include "utils.h"
#include "amd_definitions.h"
#include "hypervisor.h"

namespace MpkHooks
{
    struct MpkHook
    {
        struct MpkHook* next_hook;
        PT_ENTRY_64* the_pte;
		void* hookless_page;
		void* hooked_page;
    };


	extern int hook_count;
	extern MpkHook first_mpk_hook;

	void Init();

	MpkHook* SetMpkHook(
		void* address, 
		uint8_t* patch, 
		size_t patch_len
	);

	void HandlePageFaultMpk(
		CoreVmcbData* vcpu, 
		GPRegs* guest_regs
	);
}