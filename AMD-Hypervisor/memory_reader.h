#pragma once
#include "utils.h"

namespace MemoryUtils
{
    struct MappingWindow
    {
        void* reserved_page;
        PTE_64* reserved_page_pte;
    };

    extern MappingWindow page_map_view;

    void Init();
    
    uint8_t* MapPhysicalMem(void* phys_addr);
    uint8_t* MapVirtualMem(void* virtual_addr, CR3 context);
    void ReadVirtualMemory(void* out_buffer, size_t length, void* virtual_addr, CR3 context);
    void WriteVirtualMemory(void* in_buffer, size_t length, void* virtual_addr, CR3 context);
};

