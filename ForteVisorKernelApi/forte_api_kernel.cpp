#include "pch.h"
#include "forte_api_kernel.h"
#include <intrin.h>

namespace ForteVisor
{
    /*  Not on each core, because it's only relevant in 1 process context */
    int SetTlbHook(uintptr_t address, uint8_t* patch, size_t patch_len)
    {
        return svm_vmmcall(VMMCALL_ID::set_tlb_hook, address, patch, patch_len);
    }

    int SetNptHook(uintptr_t address, uint8_t* patch, size_t patch_len, int32_t tag)
    {
        LARGE_INTEGER length_tag;
        length_tag.LowPart = tag;
        length_tag.HighPart = patch_len;

        svm_vmmcall(VMMCALL_ID::set_npt_hook, address, patch, length_tag.QuadPart);

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

    int ForEachCore(void(*callback)(void* params), void* params)
    {
	    auto core_count = KeQueryActiveProcessorCount(0);

        for (int idx = 0; idx < core_count; ++idx)
        {
            KAFFINITY affinity = Exponent(2, idx);

            KeSetSystemAffinityThread(affinity);
            
            callback(params);
        }

        return 0;
    }

    int DisableHv()
    {
        ForEachCore(
            [](void* param) -> void {
                svm_vmmcall(VMMCALL_ID::disable_hv);
            }
        );
        return 0;
    }
};