#pragma once
#include "memory_reader.h"
#include "utils.h"
#include  "logging.h"

namespace MemoryUtils
{
    MappingWindow page_map_view;

    void Init()
    {
        CR3 cr3;
        cr3.Flags = __readcr3();

        page_map_view.reserved_page = ExAllocatePoolZero(NonPagedPool, PAGE_SIZE, 'ENON');
        page_map_view.reserved_page_pte = GetPte(page_map_view.reserved_page, cr3.AddressOfPageDirectory << PAGE_SHIFT);
        page_map_view.reserved_page_pte->Present = 0;

        Logger::Log("page_map_view.reserved_page_pte %p  page_map_view.reserved_page %p \n", page_map_view.reserved_page_pte, page_map_view.reserved_page);

        __debugbreak();
    }

    uint8_t* MapPhysical(void* phys_addr)
    {
        __invlpg(page_map_view.reserved_page);

        page_map_view.reserved_page_pte->Present = 0;
        page_map_view.reserved_page_pte->Write = 1;
        page_map_view.reserved_page_pte->PageFrameNumber = 0;

        return (uint8_t*)page_map_view.reserved_page + ((uintptr_t)phys_addr & (PAGE_SIZE - 1));
    }

    uint8_t* MapVirtual(void* virtual_addr, CR3 context)
    {
        auto guest_pte = GetPte(virtual_addr, context.AddressOfPageDirectory << PAGE_SHIFT);

        return MapPhysical((void*)(guest_pte->PageFrameNumber << PAGE_SHIFT)) + ((uintptr_t)virtual_addr & (PAGE_SIZE - 1));
    }

    PT_ENTRY_64* GetPte(void* virtual_address, uintptr_t pml4_base_pa, int (*page_table_callback)(PT_ENTRY_64*))
    {
        ADDRESS_TRANSLATION_HELPER helper;

        PT_ENTRY_64* final_entry;

        helper.AsUInt64 = (uintptr_t)virtual_address;

        PHYSICAL_ADDRESS pml4_physical;
        pml4_physical.QuadPart = pml4_base_pa;

        PML4E_64* pml4;
        PML4E_64* pml4e;

        pml4 = (PML4E_64*)MmGetVirtualForPhysical(pml4_physical);

        pml4e = &pml4[helper.AsIndex.Pml4];

        if (page_table_callback)
        {
            page_table_callback((PT_ENTRY_64*)pml4e);
        }

        if (pml4e->Present == FALSE)
        {
            return (PT_ENTRY_64*)pml4e;
        }

        PDPTE_64* pdpt;
        PDPTE_64* pdpte;

        pdpt = (PDPTE_64*)Utils::VirtualAddrFromPfn(pml4e->PageFrameNumber);

        pdpte = &pdpt[helper.AsIndex.Pdpt];

        if (page_table_callback)
        {
            page_table_callback((PT_ENTRY_64*)pdpte);
        }

        if ((pdpte->Present == FALSE) || (pdpte->LargePage != FALSE))
        {
            return (PT_ENTRY_64*)pdpte;
        }

        PDE_64* pd;
        PDE_64* pde;

        pd = (PDE_64*)Utils::VirtualAddrFromPfn(pdpte->PageFrameNumber);

        pde = &pd[helper.AsIndex.Pd];

        if (page_table_callback)
        {
            page_table_callback((PT_ENTRY_64*)pde);
        }

        if ((pde->Present == FALSE) || pde->LargePage == TRUE)
        {
            return (PT_ENTRY_64*)pde;
        }


        PTE_64* pt;
        PTE_64* pte;


        pt = (PTE_64*)Utils::VirtualAddrFromPfn(pde->PageFrameNumber);

        pte = &pt[helper.AsIndex.Pt];

        if (page_table_callback)
        {
            page_table_callback((PT_ENTRY_64*)pte);
        }

        return  (PT_ENTRY_64*)pte;
    }

};

