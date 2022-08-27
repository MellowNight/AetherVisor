#pragma once
#include "paging_utils.h"
#include "utils.h"
#include  "logging.h"

namespace PageUtils
{
    void* VirtualAddrFromPfn(uintptr_t pfn)
    {
        PHYSICAL_ADDRESS pa;
        pa.QuadPart = pfn << PAGE_SHIFT;

        return MmGetVirtualForPhysical(pa);
    }

    PFN_NUMBER	PfnFromVirtualAddr(uintptr_t va)
    {
        return MmGetPhysicalAddress((void*)va).QuadPart >> PAGE_SHIFT;
    }

    PMDL LockPages(void* virtual_address, LOCK_OPERATION  operation, KPROCESSOR_MODE access_mode)
    {
        PMDL mdl = IoAllocateMdl(virtual_address, PAGE_SIZE, FALSE, FALSE, nullptr);

        MmProbeAndLockPages(mdl, KernelMode, operation);

        return mdl;
    }

    NTSTATUS UnlockPages(PMDL mdl)
    {
        MmUnlockPages(mdl);
        IoFreeMdl(mdl);

        return STATUS_SUCCESS;
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

        pdpt = (PDPTE_64*)PageUtils::VirtualAddrFromPfn(pml4e->PageFrameNumber);

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

        pd = (PDE_64*)PageUtils::VirtualAddrFromPfn(pdpte->PageFrameNumber);

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


        pt = (PTE_64*)PageUtils::VirtualAddrFromPfn(pde->PageFrameNumber);

        pte = &pt[helper.AsIndex.Pt];

        if (page_table_callback)
        {
            page_table_callback((PT_ENTRY_64*)pte);
        }

        return  (PT_ENTRY_64*)pte;
    }

};

