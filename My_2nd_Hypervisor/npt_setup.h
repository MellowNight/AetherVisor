#pragma once
#include "Utility.h"

ULONG64	 BuildNestedPagingTables(ULONG64* NCr3, bool execute);

PTE_64*	AssignNPTEntry(PML4E_64* n_Pml4, ULONG64 PhysicalAddr, bool execute);

PVOID	AllocateNewTable(PML4E_64* PageEntry);

void	InitializeHookList(HYPERVISOR_DATA* HvData);

