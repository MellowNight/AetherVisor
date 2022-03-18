#pragma once
#include "utils.h"

namespace PhysicalReader
{
    struct MappingWindow
    {
        void* reserved_page;
        PTE_64* reserved_page_pte;
    } page_map_view;

    void Initialize()
    {
        CR3 cr3;
        cr3.Flags = __readcr3();

        for (int i = 0; i < 5; ++i)
        {
            page_map_view.reserved_page = ExAllocatePoolZero(NonPagedPool, PAGE_SIZE, i);

            page_map_view.reserved_page_pte = (PTE_64*)Utils::GetPte(page_map_views[i].reserved_page, cr3);
        }
    }
    
    uint8_t* MapPhysicalMemory(void* phys_addr)
    {
        __invlpg(page_map_view.reserved_page);

        page_map_view.reserved_page_pte->Write = true;
        page_map_view.reserved_page_pte->Present = true;
        page_map_view.reserved_page_pte->PageFrameNumber = (uintptr_t)phys_addr >> PAGE_SHIFT;

        return (uint8_t*)page_map_views.reserved_page + ((uintptr_t)phys_addr & (PAGE_SIZE - 1));
    }

    uint8_t* MapVirtualMemory(void* virtual_addr, CR3 context)
    {
        auto guest_pte = Utils::GetPte(address, context.AddressOfPageDirectory << PAGE_SHIFT);

        return MapPhysicalMemory(guest_pte->PageFrameNumber << PAGE_SHIFT);
    }
};

