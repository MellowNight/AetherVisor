#pragma once
#include "forte_api.h"

namespace Util
{
    int ForEachCore(void(*callback)(void* params), void* params);

    void WriteToReadOnly(void* address, uint8_t* bytes, size_t len);

    void TriggerCOWAndPageIn(void* address);
};

