#pragma once
#include "kernel_structs.h"

#define PAGE_ALIGN(Va) ((PVOID)((ULONG_PTR)(Va) & ~(PAGE_SIZE - 1)))

namespace Util
{
    extern "C"
    {        
        PMDL LockPages(void* virtual_address, LOCK_OPERATION operation, KPROCESSOR_MODE access_mode, int size = PAGE_SIZE);

        uintptr_t FindPattern(uintptr_t region_base, size_t region_size, const char* pattern, size_t pattern_size, char wildcard);

        void* GetKernelModule(OUT PULONG pSize, UNICODE_STRING DriverName);

        int ForEachCore(void(*callback)(void* params), void* params);

        void WriteToReadOnly(void* address, uint8_t* bytes, size_t len);

        void TriggerCOW(void* address); 
    }
};

