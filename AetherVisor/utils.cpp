#include "utils.h"
#include "logging.h"
#include "kernel_exports.h"
#include "kernel_structures.h"

namespace Utils
{
     void* PfnToVirtualAddr(uintptr_t pfn)
    {
        PHYSICAL_ADDRESS pa; 
        
        pa.QuadPart = pfn << PAGE_SHIFT;

        return MmGetVirtualForPhysical(pa);
    }

    PFN_NUMBER	PfnFromVirtualAddr(uintptr_t va)
    {
        return MmGetPhysicalAddress((void*)va).QuadPart >> PAGE_SHIFT;
    }

    PMDL LockPages(void* virtual_address, LOCK_OPERATION operation, KPROCESSOR_MODE access_mode, int size)
    {
        PMDL mdl = IoAllocateMdl(virtual_address, size, FALSE, FALSE, nullptr);

        MmProbeAndLockPages(mdl, KernelMode, operation);

        return mdl;
    }

    NTSTATUS UnlockPages(PMDL mdl)
    {
        MmUnlockPages(mdl);
        IoFreeMdl(mdl);

        return STATUS_SUCCESS;
    }

    PT_ENTRY_64* GetPte(void* virtual_address, uintptr_t pml4_base_pa, 
        int (*page_table_callback)(PT_ENTRY_64*, void*), void* callback_data)
    {
        AddressTranslationHelper helper;

        helper.as_int64 = (uintptr_t)virtual_address;

        PHYSICAL_ADDRESS pml4_physical;
        pml4_physical.QuadPart = pml4_base_pa;

        PML4E_64* pml4;
        PML4E_64* pml4e;

        pml4 = (PML4E_64*)MmGetVirtualForPhysical(pml4_physical);

        pml4e = &pml4[helper.AsIndex.pml4];

        if (page_table_callback)
        {
            page_table_callback((PT_ENTRY_64*)pml4e, callback_data);
        }

        if (pml4e->Present == FALSE)
        {
            return (PT_ENTRY_64*)pml4e;
        }

        PDPTE_64* pdpt;
        PDPTE_64* pdpte;

        pdpt = (PDPTE_64*)Utils::PfnToVirtualAddr(pml4e->PageFrameNumber);

        pdpte = &pdpt[helper.AsIndex.pdpt];

        if (page_table_callback)
        {
            page_table_callback((PT_ENTRY_64*)pdpte, callback_data);
        }

        if ((pdpte->Present == FALSE) || (pdpte->LargePage != FALSE))
        {
            return (PT_ENTRY_64*)pdpte;
        }

        PDE_64* pd;
        PDE_64* pde;

        pd = (PDE_64*)Utils::PfnToVirtualAddr(pdpte->PageFrameNumber);

        pde = &pd[helper.AsIndex.pd];

        if (page_table_callback)
        {
            page_table_callback((PT_ENTRY_64*)pde, callback_data);
        }

        if ((pde->Present == FALSE) || pde->LargePage == TRUE)
        {
            return (PT_ENTRY_64*)pde;
        }

        PTE_64* pt;
        PTE_64* pte;

        pt = (PTE_64*)Utils::PfnToVirtualAddr(pde->PageFrameNumber);

        pte = &pt[helper.AsIndex.pt];

        if (page_table_callback)
        {
            page_table_callback((PT_ENTRY_64*)pte, callback_data);
        }

        return  (PT_ENTRY_64*)pte;
    }
    
    void* GetKernelModule(size_t* out_size, UNICODE_STRING driver_name)
    {
        PLIST_ENTRY module_list = (PLIST_ENTRY)PsLoadedModuleList;

        for (PLIST_ENTRY link = module_list;
            link != module_list->Blink;
            link = link->Flink)
        {
            LDR_DATA_TABLE_ENTRY* entry = CONTAINING_RECORD(link, LDR_DATA_TABLE_ENTRY, InLoadOrderLinks);

            if (RtlCompareUnicodeString(&driver_name, &entry->BaseDllName, false) == 0)
            {
                // DbgPrint("found module! %wZ at %p \n", &entry->BaseDllName, entry->DllBase);

                if (out_size && MmIsAddressValid(out_size))
                {
                    *out_size = entry->SizeOfImage;
                }
                return entry->DllBase;
            }
        }
    }

    int ForEachCore(void(*callback)(void* params), void* params)
    {
        auto core_count = KeQueryActiveProcessorCount(0);

        for (auto idx = 0; idx < core_count; ++idx)
        {
            KAFFINITY affinity = Exponent(2, idx);

            KeSetSystemAffinityThread(affinity);

            callback(params);
        }

        return 0;
    }

    uintptr_t FindPattern(uintptr_t region_base, size_t region_size, const char* pattern, size_t pattern_size, char wildcard)
    {
        for (auto byte = (char*)region_base; byte < (char*)region_base + region_size;
            ++byte)
        {
            bool found = true;

            for (char* pattern_byte = (char*)pattern, *begin = byte; pattern_byte < pattern + pattern_size; ++pattern_byte, ++begin)
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

    void* GetKernelModule(uint32_t* out_size, UNICODE_STRING driver_name)
    {
        PLIST_ENTRY module_list = (PLIST_ENTRY)PsLoadedModuleList;

        for (PLIST_ENTRY link = module_list;
            link != module_list->Blink;
            link = link->Flink)
        {
            LDR_DATA_TABLE_ENTRY* entry = CONTAINING_RECORD(link, LDR_DATA_TABLE_ENTRY, InLoadOrderLinks);

            if (RtlCompareUnicodeString(&driver_name, &entry->BaseDllName, false) == 0)
            {
                // DbgPrint("found module! %wZ at %p \n", &entry->BaseDllName, entry->DllBase);
              
                if (out_size && MmIsAddressValid(out_size))
                {
                    *out_size = entry->SizeOfImage;
                }

                return entry->DllBase;
            }
        }

        return 0;
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