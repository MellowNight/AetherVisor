#pragma once
#include <cstdint>
#include <Windows.h>
#include <cmath>

#define PAGE_ALIGN(Va) ((PVOID)((ULONG_PTR)(Va) & ~(PAGE_SIZE - 1)))

namespace Util
{
    int ForEachCore(void(*callback)(void* params), void* params);

    void WriteToReadOnly(void* address, uint8_t* bytes, size_t len);

    void TriggerCOW(void* address);
};

