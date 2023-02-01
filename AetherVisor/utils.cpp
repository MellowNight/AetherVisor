#include "utils.h"
#include "logging.h"
#include "kernel_exports.h"
#include "kernel_structures.h"

namespace Utils
{
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