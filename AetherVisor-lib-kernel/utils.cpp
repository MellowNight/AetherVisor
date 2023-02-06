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

    void WriteToReadOnly(void* address, uint8_t* bytes, size_t len)
    {
        DWORD old_prot, old_prot2 = 0;

        SIZE_T size = len;

        auto status = ZwProtectVirtualMemory(ZwCurrentProcess(),
            (void**)&address, &size, PAGE_EXECUTE_READWRITE, &old_prot);

        DbgPrint("WriteToReadOnly status1 %p \n", status);

        memcpy((void*)address, (void*)bytes, len);


        status = ZwProtectVirtualMemory(ZwCurrentProcess(),
            (void**)&address, &size, old_prot, &old_prot2);

        DbgPrint("WriteToReadOnly status2 %p \n", status);

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
