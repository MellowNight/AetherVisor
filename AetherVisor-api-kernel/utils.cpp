#include "pch.h"
#include "utils.h"

namespace Util
{
    int Exponent(int base, int power)
    {
        int start = 1;
        for (int i = 0; i < power; ++i)
        {
            start *= base;
        }

        return start;
    }

#pragma optimize( "", off )

    int PageIn(void* address)
    {
        return *(int*)address;
    }

#pragma optimize("", on)

    int ForEachCore(void(*callback)(void* params), void* params)
    {
        auto core_count = KeQueryActiveProcessorCount(0);

        for (auto idx = 0; idx < core_count; ++idx)
        {
            KAFFINITY affinity = Util::Exponent(2, idx);

            KeSetSystemAffinityThread(affinity);

            callback(params);
        }

        return 0;
    }
};
