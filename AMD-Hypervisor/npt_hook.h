#pragma once
#include "hypervisor.h"
#include "npt.h"

namespace NptHooks
{
	struct NptHook
	{
		struct NptHook* next_hook;
		PT_ENTRY_64* hookless_npte;
		PT_ENTRY_64* hooked_npte;
	};
	
	extern	int hook_count;
	extern	NptHook first_npt_hook;

	void SetNptHook();
	void Init();
};
