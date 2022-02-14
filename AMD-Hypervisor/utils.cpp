#include "utils.h"
#include "logging.h"

namespace Utils
{
    bool IsInsideRange(uintptr_t address, uintptr_t range_base, uintptr_t range_size)
    {
        if ((range_base > address) &&
            ((range_base + range_size) < address))
        {
            return false;
        }
        else
        {
            return true;
        }
    }

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

    PT_ENTRY_64* GetPte(void* virtual_address, uintptr_t pml4_base_pa, int (*page_table_callback)(PT_ENTRY_64*))
    {
        ADDRESS_TRANSLATION_HELPER helper;

        PT_ENTRY_64* final_entry;

        helper.AsUInt64 = (uintptr_t)virtual_address;

        PHYSICAL_ADDRESS    pml4_base_physical;

        pml4_base_physical.QuadPart = pml4_base_pa;

        PML4E_64* pml4;
        PML4E_64* pml4e;

        pml4 = (PML4E_64*)MmGetVirtualForPhysical(addr);

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

        pdpt = (PDPTE_64*)VirtualAddrFromPfn(pml4e->PageFrameNumber);

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

        pd = (PDE_64*)VirtualAddrFromPfn(pdpte->PageFrameNumber);

        pde = &pd[helper.AsIndex.Pd];

        if (page_table_callback)
        {
            page_table_callback((PT_ENTRY_64*)pde);
        }

        if ((pde->Present == FALSE) || (pde->LargePage != FALSE))
        {
            return (PT_ENTRY_64*)pde;
        }


        PTE_64* pt;
        PTE_64* pte;

        
        pt = (PTE_64*)VirtualAddrFromPfn(pde->PageFrameNumber);

        pte = &pt[helper.AsIndex.Pt];

        if (page_table_callback)
        {
            page_table_callback((PT_ENTRY_64*)pte);
        }

        return  (PT_ENTRY_64*)pte;
    }
    
    PT_ENTRY_64* GetPte(void* virtual_address, uintptr_t pml4_base_pa, PDPTE_64** pdpte_result, PDE_64** pde_result)
    {
        ADDRESS_TRANSLATION_HELPER helper;

        PT_ENTRY_64* final_entry;

        helper.AsUInt64 = (uintptr_t)virtual_address;

        PHYSICAL_ADDRESS    pml4_base_physical;

        pml4_base_physical.QuadPart = pml4_base_pa;


        PML4E_64* pml4;
        PML4E_64* pml4e;

        pml4 = (PML4E_64*)MmGetVirtualForPhysical(pml4_base_physical);

        pml4e = &pml4[helper.AsIndex.Pml4];

        if (pml4e->Present == FALSE)
        {
            return (PT_ENTRY_64*)pml4e;
        }


        PDPTE_64* pdpt;
        PDPTE_64* pdpte;

        pdpt = (PDPTE_64*)VirtualAddrFromPfn(pml4e->PageFrameNumber);

        *PdpteResult = pdpte = &pdpt[helper.AsIndex.Pdpt];

        if ((pdpte->Present == FALSE) || (pdpte->LargePage != FALSE))
        {
            return (PT_ENTRY_64*)pdpte;
        }


        PDE_64* pd;
        PDE_64* pde;

        pd = (PDE_64*)VirtualAddrFromPfn(pdpte->PageFrameNumber);

        *PdeResult = pde = &pd[helper.AsIndex.Pd];

        if ((pde->Present == FALSE) || (pde->LargePage != FALSE))
        {
            return (PT_ENTRY_64*)pde;
        }


        PTE_64* pt;
        PTE_64* pte;


        pt = (PTE_64*)VirtualAddrFromPfn(pde->PageFrameNumber);

        pte = &pt[helper.AsIndex.Pt];

        return  (PT_ENTRY_64*)pte;
    }

    void GetJmpCode(uintptr_t jmp_target, uint8_t* output)
    {
        char jmp_rip[15] = "\xFF\x25\x00\x00\x00\x00\xCC\xCC\xCC\xCC\xCC\xCC\xCC\xCC";

        memcpy(jmp_rip + 6, &jmp_target, sizeof(uintptr_t)));
        memcpy((void*)output, jmp_rip, 14);
    }

    PMDL LockPages(void* virtual_address, LOCK_OPERATION  operation)
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

    void* GetDriverBaseAddress(OUT PULONG pSize, UNICODE_STRING DriverName)
    {
        PLIST_ENTRY moduleList = (PLIST_ENTRY)PsLoadedModuleList;

        UNICODE_STRING  DrvName;

        for (PLIST_ENTRY link = moduleList; 
            link != moduleList->Blink;
            link = link->Flink)
        {
            LDR_DATA_TABLE_ENTRY* entry = CONTAINING_RECORD(link, LDR_DATA_TABLE_ENTRY, InLoadOrderLinks);

            if (RtlCompareUnicodeString(&DriverName, &entry->BaseDllName, false) == 0)
            {
                DbgPrint("found module! \n");
                if (pSize && MmIsAddressValid(pSize))
                {
                    *pSize = entry->SizeOfImage;
                }

                return entry->DllBase;
            }
        }

        return 0;
    }

	HANDLE GetProcessId(const wchar_t* process_name);
    {
        auto list_entry = (LIST_ENTRY*)(((uintptr_t)PsInitialSystemProcess) + OFFSET::processLinksOffset);

        auto current_entry = list_entry->Flink;

        while (current_entry != list_entry && current_entry != NULL)
        {
            auto process = (PEPROCESS)((uintptr_t)current_entry - OFFSET::processLinksOffset);

            if (!wcscmp(PsGetProcessImageFileName(process), process_name))
            {
                DbgPrint("found process!! PEPROCESS value %p \n", process);

                return process;
            }

            current_entry = current_entry->Flink;
        }
    }
    
    Exponent(int base, int power)
    {
        int start = 1;
        for (int i = 0; i < power; ++i)
        {
            start *= base;
        }

        return start;
    }
}