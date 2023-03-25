#include "utils.h"

namespace Util
{
    extern "C"
    {    
        PMDL LockPages(void* virtual_address, LOCK_OPERATION operation, KPROCESSOR_MODE access_mode, int size)
        {
            PMDL mdl = IoAllocateMdl(virtual_address, size, FALSE, FALSE, nullptr);

            MmProbeAndLockPages(mdl, KernelMode, operation);

            return mdl;
        }

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

        void* GetKernelModule(OUT PULONG pSize, UNICODE_STRING DriverName)
        {
            PLIST_ENTRY moduleList = (PLIST_ENTRY)PsLoadedModuleList;

            for (PLIST_ENTRY link = moduleList; link; link = link->Flink)
            {
                LDR_DATA_TABLE_ENTRY* entry = CONTAINING_RECORD(link, LDR_DATA_TABLE_ENTRY, InLoadOrderLinks);

                if (RtlCompareUnicodeString(&DriverName, &entry->BaseDllName, false) == 0)
                {
                    DbgPrint("found module! %wZ at %p \n", &entry->BaseDllName, entry->DllBase);
                    if (pSize && MmIsAddressValid(pSize))
                    {
                        *pSize = entry->SizeOfImage;
                    }

                    return entry->DllBase;
                }

                if (link == moduleList->Blink)
                {
                    break;
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

        void WriteToReadOnly(void* address, uint8_t* bytes, size_t len)
        {
            DWORD old_prot, old_prot2 = 0;

            SIZE_T size = len;

            auto status = ZwProtectVirtualMemory(ZwCurrentProcess(),
                (void**)&address, &size, PAGE_EXECUTE_READWRITE, &old_prot);

            //  DbgPrint("WriteToReadOnly status1 %p \n", status);

            memcpy((void*)address, (void*)bytes, len);


            status = ZwProtectVirtualMemory(ZwCurrentProcess(),
                (void**)&address, &size, old_prot, &old_prot2);

            //   DbgPrint("WriteToReadOnly status2 %p \n", status);

        }

#pragma optimize( "", off )

        void TriggerCOW(void* address)
        {
            auto buffer = *(uint8_t*)address;

            /*	trigger COW	*/

            WriteToReadOnly(address, (uint8_t*)"\xC3", 1);
            WriteToReadOnly(address, &buffer, 1);
        }
#pragma optimize( "", on )
    }
};
