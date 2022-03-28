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

        page_map_view.reserved_page = MmAllocateMappingAddress(PAGE_SIZE, 'ENON');
        page_map_view.reserved_page_pte = (PTE_64*)GetSystemCtxPte(page_map_view.reserved_page, cr3.AddressOfPageDirectory << PAGE_SHIFT);
    }

    uint8_t* MapPhysicalMem(void* phys_addr)
    {
        __invlpg(page_map_view.reserved_page);

        page_map_view.reserved_page_pte->Write = true;
        page_map_view.reserved_page_pte->Present = true;
        page_map_view.reserved_page_pte->PageFrameNumber = (uintptr_t)phys_addr >> PAGE_SHIFT;

        return (uint8_t*)page_map_view.reserved_page + ((uintptr_t)phys_addr & (PAGE_SIZE - 1));
    }

    uint8_t* MapVirtualMem(void* virtual_addr, CR3 context)
    {
        auto guest_pte = GetPte(virtual_addr, context.AddressOfPageDirectory << PAGE_SHIFT);

        return MapPhysicalMem((void*)(guest_pte->PageFrameNumber << PAGE_SHIFT));
    }

    void ReadVirtualMemory(void* out_buffer, size_t length, void* virtual_addr, CR3 context)
    {
        memcpy(out_buffer, MapVirtualMem(virtual_addr, context), length);
    }

    void WriteVirtualMemory(void* in_buffer, size_t length, void* virtual_addr, CR3 context)
    {
        memcpy(MapVirtualMem(virtual_addr, context), in_buffer, length);
    }

    PT_ENTRY_64* GetPte(void* virtual_address, uintptr_t pml4_base_pa, int (*page_table_callback)(PT_ENTRY_64*))
    {
        Logger::Log("\n \n \n \n virtual_address %p pml4_base_pa %p \n \n", virtual_address, pml4_base_pa);

        ADDRESS_TRANSLATION_HELPER helper;

        PT_ENTRY_64* final_entry;

        helper.AsUInt64 = (uintptr_t)virtual_address;

        PML4E_64* pml4;
        PML4E_64* pml4e;

        pml4 = (PML4E_64*)MapPhysicalMem((void*)pml4_base_pa);

        pml4e = &pml4[helper.AsIndex.Pml4];
        Logger::Log("pml4e->PageFrameNumber << PAGE_SHIFT %p pml4e %p \n", pml4e->PageFrameNumber << PAGE_SHIFT, pml4e);

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

        pdpt = (PDPTE_64*)MapPhysicalMem((void*)(pml4e->PageFrameNumber << PAGE_SHIFT));

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
        Logger::Log("pdpte->PageFrameNumber << PAGE_SHIFT %p \n", pdpte->PageFrameNumber << PAGE_SHIFT);

        pd = (PDE_64*)MapPhysicalMem((void*)(pdpte->PageFrameNumber << PAGE_SHIFT));

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

        Logger::Log("pde->PageFrameNumber << PAGE_SHIFT %p \n", pde->PageFrameNumber << PAGE_SHIFT);

        pt = (PTE_64*)MapPhysicalMem((void*)(pde->PageFrameNumber << PAGE_SHIFT));

        pte = &pt[helper.AsIndex.Pt];

        if (page_table_callback)
        {
            page_table_callback((PT_ENTRY_64*)pte);
        }

        return  (PT_ENTRY_64*)pte;
    }

    PT_ENTRY_64* GetSystemCtxPte(void* virtual_address, uintptr_t pml4_base_pa, int (*page_table_callback)(PT_ENTRY_64*))
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

