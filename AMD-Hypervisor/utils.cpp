#include "utils.h"
#include "logging.h"
#include "kernel_exports.h"

namespace Utils
{
    uintptr_t FindPattern(uintptr_t region_base, size_t region_size, const char* pattern, size_t pattern_size, char wildcard)
	{
        for (auto byte = (char*)region_base;
            byte < (char*)region_base + region_size;
            ++byte)
        {
            bool found = true;

            for (char* pattern_byte = (char*)pattern, *begin = byte;
                pattern_byte < pattern + pattern_size;
                ++pattern_byte, ++begin)
            {
                if (*pattern_byte != *begin && *pattern_byte != wildcard)
                {
                    found = false;
                }
            }

            if (found)
            {
                return (uintptr_t)byte;
            }
        }

		return 0;
	}

    KIRQL DisableWP()
    {
        KIRQL	tempirql = KeRaiseIrqlToDpcLevel();

        ULONG64  cr0 = __readcr0();

        cr0 &= 0xfffffffffffeffff;

        __writecr0(cr0);

        _disable();

        return tempirql;
    }

    void EnableWP(KIRQL	tempirql)
    {
        ULONG64	cr0 = __readcr0();

        cr0 |= 0x10000;

        _enable();

        __writecr0(cr0);

        KeLowerIrql(tempirql);
    }

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

        pml4 = (PML4E_64*)MmGetVirtualForPhysical(pml4_base_physical);

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

        *pdpte_result = pdpte = &pdpt[helper.AsIndex.Pdpt];

        if ((pdpte->Present == FALSE) || (pdpte->LargePage != FALSE))
        {
            return (PT_ENTRY_64*)pdpte;
        }


        PDE_64* pd;
        PDE_64* pde;

        pd = (PDE_64*)VirtualAddrFromPfn(pdpte->PageFrameNumber);

        *pde_result = pde = &pd[helper.AsIndex.Pd];

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

    void* GetDriverBaseAddress(size_t* out_driver_size, UNICODE_STRING driver_name)
    {
        auto moduleList = (PLIST_ENTRY)PsLoadedModuleList;

        UNICODE_STRING  DrvName;

        for (PLIST_ENTRY link = moduleList; link != moduleList->Blink; link = link->Flink)
        {
            LDR_DATA_TABLE_ENTRY* entry = CONTAINING_RECORD(link, LDR_DATA_TABLE_ENTRY, InLoadOrderLinks);

            if (RtlCompareUnicodeString(&driver_name, &entry->BaseDllName, false) == 0)
            {
                Logger::Log("found module! \n");
                if (out_driver_size && MmIsAddressValid(out_driver_size))
                {
                    *out_driver_size = entry->SizeOfImage;
                }

                return entry->DllBase;
            }
        }

        return 0;
    }

	HANDLE GetProcessId(const char* process_name)
    {
        auto list_entry = (LIST_ENTRY*)(((uintptr_t)PsInitialSystemProcess) + OFFSET::ProcessLinksOffset);

        auto current_entry = list_entry->Flink;

        while (current_entry != list_entry && current_entry != NULL)
        {
            auto process = (PEPROCESS)((uintptr_t)current_entry - OFFSET::ProcessLinksOffset);

            if (!strcmp(PsGetProcessImageFileName(process), process_name))
            {
                Logger::Log("found process!! PEPROCESS value %p \n", process);

                return process;
            }

            current_entry = current_entry->Flink;
        }
    }
    
    int Exponent(int base, int power)
    {
        int start = 1;
        for (int i = 0; i < power; ++i)
        {
            start *= base;
        }

        return start;
    }
}