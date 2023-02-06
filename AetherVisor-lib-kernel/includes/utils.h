#pragma once
#include    <ntifs.h>
#include    <Ntstrsafe.h>
#include    <intrin.h>
#include	<cstdint>
//#include    <ntdef.h>
//#include    <windef.h>
//#include    <wdm.h>
//#include	<TraceLoggingProvider.h>
//

#define PAGE_ALIGN(Va) ((PVOID)((ULONG_PTR)(Va) & ~(PAGE_SIZE - 1)))

extern "C"
{
    NTSYSAPI NTSTATUS NTAPI ZwProtectVirtualMemory(
        IN HANDLE ProcessHandle,
        IN OUT PVOID* BaseAddress,
        IN OUT SIZE_T* NumberOfBytesToProtect,
        IN uint32_t NewAccessProtection,
        OUT PULONG OldAccessProtection
    );
}

namespace Util
{
    int Exponent(int base, int power);

    int ForEachCore(void(*callback)(void* params), void* params);

    void WriteToReadOnly(void* address, uint8_t* bytes, size_t len);

    void TriggerCOW(void* address);
};

