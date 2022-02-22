NptHooks#pragma once
#include "hypervisor.h"
#include "npt.h"

namespace NptHooker
{
	struct NptHook
	{
		struct NptHook* next_hook;
		PT_ENTRY_64* hookless_npte;
		PT_ENTRY_64* hooked_npte;
	};

	extern NptHook* first_npt_hook;

	NptHook* FindByHooklessPhysicalPage(
		uint64_t page_physical
	);

	void Init();
};
