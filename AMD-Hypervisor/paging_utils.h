#pragma once
#include "utils.h"

namespace PageUtils
{
    /*
        virtual_addr - virtual address to get pte of
        pml4_base_pa - base physical address of PML4
        page_table_callback - callback to call on the PTE, PDPTE, PDE, and PML4E associated
        with this virtual address
    */
    void* VirtualAddrFromPfn(uintptr_t pfn);

    PFN_NUMBER	PfnFromVirtualAddr(uintptr_t va);

    PMDL LockPages(void* virtual_address, LOCK_OPERATION  operation, KPROCESSOR_MODE access_mode);

    NTSTATUS UnlockPages(PMDL mdl);

    PT_ENTRY_64* GetPte(
        void* virtual_address, 
        uintptr_t pml4_base_pa, 
        int (*page_table_callback)(PT_ENTRY_64*) = NULL
    );
};

