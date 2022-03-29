#pragma once
#include "utils.h"

namespace MemoryUtils
{
    struct MappingWindow
    {
        void* reserved_page;
        PT_ENTRY_64* reserved_page_pte;
    };

    extern MappingWindow page_map_view;

    void Init();
    
    uint8_t* MapPhysical(void* phys_addr);
    uint8_t* MapVirtual(void* virtual_addr, CR3 context);

    /*
        virtual_addr - virtual address to get pte of
        pml4_base_pa - base physical address of PML4
        page_table_callback - callback to call on the PTE, PDPTE, PDE, and PML4E associated
        with this virtual address
    */

    PT_ENTRY_64* GetPte(
        void* virtual_address, 
        uintptr_t pml4_base_pa, 
        int (*page_table_callback)(PT_ENTRY_64*) = NULL
    );
};

