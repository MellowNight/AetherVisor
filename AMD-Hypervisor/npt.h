#pragma once
#include "utils.h"
#include "hypervisor.h"

struct PTEAccess
{
    bool present;
    bool writable;
    bool execute;
};

uintptr_t BuildNestedPagingTables(
    uintptr_t* NCr3, 
    PTEAccess flags
);

PTE_64*	AssignNPTEntry(
    PML4E_64* n_Pml4, 
    uintptr_t PhysicalAddr, 
    PTEAccess flags
);

void* AllocateNewTable(
    PML4E_64* PageEntry
);

void HandleNestedPageFault(
	VcpuData* VpData,
	GuestRegisters* GuestContext
);

enum NCR3_DIRECTORIES
{
    primary,
    noexecute,
    sandbox,
    sandbox_single_step
};
