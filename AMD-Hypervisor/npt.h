#pragma once
#include "utils.h"
#include "hypervisor.h"

uintptr_t BuildNestedPagingTables(uintptr_t* NCr3, bool execute);

PTE_64*	AssignNPTEntry(PML4E_64* n_Pml4, uintptr_t PhysicalAddr, bool execute);

void* AllocateNewTable(PML4E_64* PageEntry);

void HandleNestedPageFault(
	CoreData* VpData,
	GPRegs* GuestContext
);