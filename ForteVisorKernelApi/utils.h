#pragma once
#include "forte_api_kernel.h"

namespace Util
{
    int ForEachCore(void(*callback)(void* params), void* params);

    int PageIn(void* address);
};

