#pragma once
#include "utils.h"

struct NptHookEntry
{
	LIST_ENTRY	npt_hook_list;
	PT_ENTRY_64* innocent_npt_entry;
	PT_ENTRY_64* hooked_npt_entry;
};

NptHookEntry*	GetHookByPhysicalPage(GlobalHvData* HvData, UINT64 PagePhysical);
NptHookEntry*	GetHookByOldFuncAddress(GlobalHvData* HvData, void*	FuncAddr);
NptHookEntry*	AddHookedPage(GlobalHvData* HvData, void* PhysicalAddr, uintptr_t	NCr3, char* patch, int PatchLen);

void	SetHooks();
