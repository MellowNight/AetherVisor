#pragma once
#include "utils.h"
#include "amd_definitions.h"
#include "hypervisor.h"

namespace TlbHooker
{
	// ret_pointer = function pointer to 0xC3 on page, called to trigger an execute access on the target page

	struct SplitTlbHook
	{
		struct SplitTlbHook* next_hook;
		PT_ENTRY_64* hookless_pte;
		PT_ENTRY_64* hooked_pte;
		void* hooked_page_va;
		uint8_t* exec_gadget;
		uint8_t* read_gadget;
	};

	extern int hook_count;
	extern SplitTlbHook first_tlb_hook;

	void Init();

	void HandlePageFaultTlb(
		CoreVmcbData* vcpu,
		GPRegs* guest_regs
	);

	SplitTlbHook* SetTlbHook(
		void* address, 
		uint8_t* patch, 
		size_t patch_len
	);

}

