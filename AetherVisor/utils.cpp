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

    PFN_NUMBER VirtualAddrToPfn(uintptr_t va)
    {
        return MmGetPhysicalAddress((void*)va).QuadPart >> PAGE_SHIFT;
    }

    PMDL LockPages(void* virtual_address, LOCK_OPERATION operation, KPROCESSOR_MODE access_mode, int size)
    {
        PMDL mdl = IoAllocateMdl(virtual_address, size, FALSE, FALSE, nullptr);

        MmProbeAndLockPages(mdl, KernelMode, operation);

        return mdl;
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
    NTSTATUS UnlockPages(PMDL mdl)
    {
        MmUnlockPages(mdl);
        IoFreeMdl(mdl);

        return STATUS_SUCCESS;
    }

    void* GetUserModule32(PEPROCESS pProcess, PUNICODE_STRING ModuleName)
    {
        LARGE_INTEGER time = { 0 };
        time.QuadPart = -250ll * 10 * 1000;     // 250 msec.

        PPEB32 pPeb32 = (PPEB32)PsGetProcessWow64Process(pProcess);
        if (pPeb32 == NULL)
        {
            DbgPrint("BlackBone: %s: No PEB present. Aborting\n", __FUNCTION__);
            return NULL;
        }

        // Wait for loader a bit
        for (INT i = 0; !pPeb32->Ldr && i < 10; i++)
        {
            DbgPrint("BlackBone: %s: Loader not intialiezd, waiting\n", __FUNCTION__);
            KeDelayExecutionThread(KernelMode, TRUE, &time);
        }

        // Still no loader
        if (!pPeb32->Ldr)
        {
            DbgPrint("BlackBone: %s: Loader was not intialiezd in time. Aborting\n", __FUNCTION__);
            return NULL;
        }

        // Search in InLoadOrderModuleList
        for (PLIST_ENTRY32 pListEntry = (PLIST_ENTRY32)((PPEB_LDR_DATA32)pPeb32->Ldr)->InLoadOrderModuleList.Flink;
            pListEntry != &((PPEB_LDR_DATA32)pPeb32->Ldr)->InLoadOrderModuleList;
            pListEntry = (PLIST_ENTRY32)pListEntry->Flink)
        {
            UNICODE_STRING ustr;
            PLDR_DATA_TABLE_ENTRY32 pEntry = CONTAINING_RECORD(pListEntry, LDR_DATA_TABLE_ENTRY32, InLoadOrderLinks);

            RtlUnicodeStringInit(&ustr, (PWCH)pEntry->BaseDllName.Buffer);

            if (RtlCompareUnicodeString(&ustr, ModuleName, TRUE) == 0)
                return (PVOID)pEntry->DllBase;
        }
    }

    uintptr_t GetModuleFromAddress32(PEPROCESS pProcess, uintptr_t address, PUNICODE_STRING ModuleName)
    {
        LARGE_INTEGER time = { 0 };
        time.QuadPart = -250ll * 10 * 1000;     // 250 msec.

        PPEB32 pPeb32 = (PPEB32)PsGetProcessWow64Process(pProcess);
        if (pPeb32 == NULL)
        {
            DbgPrint("BlackBone: %s: No PEB present. Aborting\n", __FUNCTION__);
            return NULL;
        }

        // Wait for loader a bit
        for (INT i = 0; !pPeb32->Ldr && i < 10; i++)
        {
            DbgPrint("BlackBone: %s: Loader not intialiezd, waiting\n", __FUNCTION__);
            KeDelayExecutionThread(KernelMode, TRUE, &time);
        }

        // Still no loader
        if (!pPeb32->Ldr)
        {
            DbgPrint("BlackBone: %s: Loader was not intialiezd in time. Aborting\n", __FUNCTION__);
            return NULL;
        }

        // Search in InLoadOrderModuleList
        for (PLIST_ENTRY32 pListEntry = (PLIST_ENTRY32)((PPEB_LDR_DATA32)pPeb32->Ldr)->InLoadOrderModuleList.Flink;
            pListEntry != &((PPEB_LDR_DATA32)pPeb32->Ldr)->InLoadOrderModuleList;
            pListEntry = (PLIST_ENTRY32)pListEntry->Flink)
        {
            PLDR_DATA_TABLE_ENTRY32 pEntry = CONTAINING_RECORD(pListEntry, LDR_DATA_TABLE_ENTRY32, InLoadOrderLinks);

            if (address > pEntry->DllBase && (address < (pEntry->DllBase + pEntry->SizeOfImage)))
            {
                RtlUnicodeStringInit(ModuleName, (PWCH)pEntry->BaseDllName.Buffer);

                return pEntry->DllBase;
            }

        }

        return NULL;
    }

    void* GetUserModule(PEPROCESS pProcess, PUNICODE_STRING ModuleName, PPEB peb)
    {
        if (!peb)
        {
            return NULL;
        }

        // Still no loader
        while (!peb->Ldr)
        {
            DbgPrint("pPeb->Ldr NULL \n");

            LARGE_INTEGER interval;
            interval.QuadPart = -1 * 10 * 1000 * 10; /* 1/100 second */
            KeDelayExecutionThread(KernelMode, FALSE, &interval);
        }

        // Search in InLoadOrderModuleList
        for (PLIST_ENTRY pListEntry = peb->Ldr->InLoadOrderModuleList.Flink;
            pListEntry != &peb->Ldr->InLoadOrderModuleList;
            pListEntry = pListEntry->Flink)
        {
            LDR_DATA_TABLE_ENTRY* pEntry = CONTAINING_RECORD(pListEntry, LDR_DATA_TABLE_ENTRY, InLoadOrderLinks);

            if (RtlCompareUnicodeString(&pEntry->BaseDllName, ModuleName, FALSE) == 0)
            {
                DbgPrint("found process module %wZ \n", &pEntry->BaseDllName);

                return pEntry->DllBase;
            }
        }

        return NULL;
    }


    PT_ENTRY_64* GetPte(void* virtual_address, uintptr_t pml4_base_pa, int (*page_table_callback)(PT_ENTRY_64*, void*), void* callback_data)
    {
        AddressTranslationHelper helper;

        helper.as_int64 = (uintptr_t)virtual_address;

        PHYSICAL_ADDRESS pml4_physical;
        pml4_physical.QuadPart = pml4_base_pa;

        PML4E_64* pml4;
        PML4E_64* pml4e;

        pml4 = (PML4E_64*)MmGetVirtualForPhysical(pml4_physical);


        if (pml4 == NULL)
        {
            return NULL;
        }

        pml4e = &pml4[helper.AsIndex.pml4];

        if (page_table_callback)
        {
            page_table_callback((PT_ENTRY_64*)pml4e, callback_data);
        }
        if (pml4e->Present == FALSE)
        {
            return NULL;
        }

        PDPTE_64* pdpt;
        PDPTE_64* pdpte;

        pdpt = (PDPTE_64*)Utils::PfnToVirtualAddr(pml4e->PageFrameNumber);

        if (pdpt == NULL)
        {
            return NULL;
        }

        pdpte = &pdpt[helper.AsIndex.pdpt];

        if (page_table_callback)
        {
            page_table_callback((PT_ENTRY_64*)pdpte, callback_data);
        }

        if ((pdpte->LargePage == TRUE))
        {
            return (PT_ENTRY_64*)pdpte;
        }
        else if (pdpte->Present == FALSE)
        {
            return NULL;
        }

        PDE_64* pd;
        PDE_64* pde;

        pd = (PDE_64*)Utils::PfnToVirtualAddr(pdpte->PageFrameNumber);

        if (pd == NULL)
        {
            return NULL;
        }

        pde = &pd[helper.AsIndex.pd];

        if (page_table_callback)
        {
            page_table_callback((PT_ENTRY_64*)pde, callback_data);
        }

        if (pde->LargePage == TRUE)
        {
            return (PT_ENTRY_64*)pde;
        }
        else if (pde->Present == FALSE)
        {
            return NULL;
        }

        PTE_64* pt;
        PTE_64* pte;

        pt = (PTE_64*)Utils::PfnToVirtualAddr(pde->PageFrameNumber);

        if (pt == NULL)
        {
            return NULL;
        }

        pte = &pt[helper.AsIndex.pt];

        if (page_table_callback)
        {
            page_table_callback((PT_ENTRY_64*)pte, callback_data);
        }

        if (pte->Present == FALSE)
        {
            return NULL;
        }

        return  (PT_ENTRY_64*)pte;
    }
    
    void* GetKernelModule(size_t* out_size, UNICODE_STRING driver_name)
    {
        auto module_list = (PLIST_ENTRY)PsLoadedModuleList;

        for (auto link = module_list; link != module_list->Blink; link = link->Flink)
        {
            LDR_DATA_TABLE_ENTRY* entry = CONTAINING_RECORD(link, LDR_DATA_TABLE_ENTRY, InLoadOrderLinks);

            if (RtlCompareUnicodeString(
                &driver_name, &entry->BaseDllName, false) == 0)
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