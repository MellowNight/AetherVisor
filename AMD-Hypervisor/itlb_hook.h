#pragma once
#include "utils.h"
#include "amd_definitions.h"
#include "hypervisor.h"

namespace TlbHooks
{
	struct SplitTlbHook
	{
		struct SplitTlbHook* next_hook;
		PT_ENTRY_64* hookless_pte;
		PT_ENTRY_64* hooked_pte;
		void* hooked_page_va;
		uintptr_t page_fault_rip;	/*	saved rip at page fault	*/
		uint8_t* exec_gadget;
		uint8_t* read_gadget;
	};

	extern int hook_count;
	extern SplitTlbHook first_tlb_hook;

	void Init();

	bool HandlePageFaultTlb(
		CoreVmcbData* vcpu,
		GPRegs* guest_regs
	);


	void HandleTlbHookBreakpoint(
		CoreVmcbData* vcpu
	);

	SplitTlbHook* SetTlbHook(
		void* address, 
		uint8_t* patch, 
		size_t patch_len
	);

}

