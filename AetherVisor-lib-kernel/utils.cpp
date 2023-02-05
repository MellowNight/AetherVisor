#include "utils.h"

namespace Util
{
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

    int Exponent(int base, int power)
    {
        int start = 1;
        for (int i = 0; i < power; ++i)
        {
            start *= base;
        }

        return start;
    }

    void WriteToReadOnly(void* address, uint8_t* bytes, size_t len)
    {
        DWORD old_prot, old_prot2 = 0;

        SIZE_T size = len;

        auto status = ZwProtectVirtualMemory(ZwCurrentProcess(),
            (void**)&address, &size, PAGE_EXECUTE_READWRITE, &old_prot);

        memcpy((void*)address, (void*)bytes, len);


        status = ZwProtectVirtualMemory(ZwCurrentProcess(),
            (void**)&address, &size, old_prot, &old_prot2);
    }

#pragma optimize( "", off )

    void TriggerCOW(void* address)
    {
        uint8_t buffer;

        /*	trigger COW	*/

        WriteToReadOnly(address, (uint8_t*)"\xC3", 1);
        WriteToReadOnly(address, &buffer, 1);
    }
#pragma optimize( "", on )
};
