#include "pch.h"
#include "forte_hv_api.h"

namespace ForteVisor
{
    /*  Not on each core, because it's only relevant in 1 process context */
    int SetTlbHook(uintptr_t address, uint8_t* patch, size_t patch_len)
    {
        return vmmcall(VMMCALL_ID::set_tlb_hook, address, patch, patch_len);
    }

    int SetMpkHook(uintptr_t address, uint8_t* patch, size_t patch_len)
    {
        return vmmcall(VMMCALL_ID::set_mpk_hook, address, patch, patch_len);
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

    int ForEachCore(void(*callback)())
    {
	    auto core_count = KeQueryActiveProcessorCount(0);

        for (int idx = 0; idx < core_count; ++idx)
        {
            KAFFINITY affinity = Exponent(2, idx);

            KeSetSystemAffinityThread(affinity);
            auto affinity = pow(2, idx);
            
            SetThreadAffinityMask(GetCurrentThread(), affinity);

            callback();
        }

        return 0;
    }

    int DisableHv()
    {
        ForEachCore(
            []() -> void {
                vmmcall(VMMCALL_ID::disable_hv);
            }
        );
        return 0;
    }
};