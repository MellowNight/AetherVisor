#include "utils.h"

namespace Util
{
    void WriteToReadOnly(void* address, uint8_t* bytes, size_t len)
    {
        DWORD old_prot;
        VirtualProtect((LPVOID)address, len, PAGE_EXECUTE_READWRITE, &old_prot);
        memcpy((void*)address, (void*)bytes, len);
        VirtualProtect((LPVOID)address, len, old_prot, 0);
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
