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
    
    T ReadPhysicalMemory(void* phys_addr)
    {
        __invlpg(page_map_view.reserved_page);

        page_map_view.reserved_page_pte->Write = true;
        page_map_view.reserved_page_pte->Present = true;
        page_map_view.reserved_page_pte->PageFrameNumber = (uintptr_t)phys_addr >> PAGE_SHIFT;

        return (uintptr_t)page_map_views.reserved_page + ((uintptr_t)phys_addr & (PAGE_SIZE - 1));
    }

    T ReadVirtualMemory(void* virtual_addr, CR3 context)
    {
        CR3 cr3;
        cr3.flags = __readcr3;

        auto guest_pte = Utils::GetPte(address, VpData->guest_vmcb.save_state_area.Cr3);

        __invlpg(page_map_view.reserved_page);

        page_map_view.reserved_page_pte->Write = true;
        page_map_view.reserved_page_pte->Present = true;
        page_map_view.reserved_page_pte->PageFrameNumber = (uintptr_t)phys_addr >> PAGE_SHIFT;

        return (uintptr_t)page_map_views.reserved_page + ((uintptr_t)phys_addr & (PAGE_SIZE - 1));
    }
};

