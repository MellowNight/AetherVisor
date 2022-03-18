#pragma once
#include "utils.h"

namespace PhysicalReader
{
    struct MappingWindow
    {
        void* reserved_page;
        PTE_64* reserved_page_pte;
    };

    MappingWindow page_map_views[5];

    void Initialize()
    {
        CR3 cr3;

        cr3.Flags = __readcr3();

        for (int i = 0; i < 5; ++i)
        {
            page_map_views[i].reserved_page = ExAllocatePoolZero(PAGE_SIZE, i);

            page_map_views[i].reserved_page_pte = (PTE_64*)Utils::GetPte(page_map_views[i].reserved_page, cr3);
        }
    }

    PVOID mapPage(PVOID physicalAddr, int id, CR3 cr3)
    {
        page_map_views[id].reserved_page_pte->Write = true;
        page_map_views[id].reserved_page_pte->Present = true;

        page_map_views[id].reserved_page_pte->PageFrameNumber = (DWORD64)physicalAddr >> PAGE_SHIFT;

        __invlpg(page_map_views[id].reserved_page);

        return (PVOID)((DWORD64)(page_map_views[id].reserved_page) + ((DWORD64)physicalAddr & PAGE_MASK));
    }




    PVOID    UnmapPage(PVOID   physicalAddr, int id)
    {
        page_map_views[id].reserved_page_pte->Flags = 0;

        __invlpg(page_map_views[id].reserved_page);

        return 0;
    }


};

