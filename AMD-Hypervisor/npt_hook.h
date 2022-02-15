#pragma once
#include "utils.h"

struct NptHookEntry
{
	LIST_ENTRY	npt_hook_list;
	PT_ENTRY_64* innocent_npte;
	PT_ENTRY_64* hooked_npte;
	bool execute_only;
};

NptHookEntry* GetHookByPhysicalPage(
	Hypervisor* HvData, 
	UINT64 PagePhysical
);

NptHookEntry* GetHookByOldFuncAddress(
	Hypervisor* HvData,
	void*	FuncAddr
);
NptHookEntry* AddHookedPage(
	Hypervisor* HvData, 
	void* PhysicalAddr, 
	uintptr_t	NCr3, 
	char* patch, 
	int PatchLen
);

void	SetHooks();
