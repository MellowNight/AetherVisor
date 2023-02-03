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
    PT_ENTRY_64* page_entry
);

enum NCR3_DIRECTORIES
{
    primary,
    shadow,
    sandbox,
    sandbox_single_step
};
