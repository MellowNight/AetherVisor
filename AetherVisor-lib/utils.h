#pragma once
#include "aethervisor.h"

namespace Util
{
    int ForEachCore(void(*callback)(void* params), void* params);

    void WriteToReadOnly(void* address, uint8_t* bytes, size_t len);

    void TriggerCOW(void* address);
};

