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
    uintptr_t* ncr3, 
    PTEAccess flags
);

PTE_64*	AssignNptEntry(
    PML4E_64* n_Pml4, 
    uintptr_t PhysicalAddr, 
    PTEAccess flags
);

void* AllocateNewTable(
    PML4E_64* PageEntry
);

void NestedPageFaultHandler(
	VcpuData* VpData,
	GuestRegs* GuestContext
);

enum NCR3_DIRECTORIES
{
    primary,
    noexecute,
    sandbox,
    sandbox_single_step
};
