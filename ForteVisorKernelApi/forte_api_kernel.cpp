#include "pch.h"
#include "forte_api_kernel.h"
#include <intrin.h>

namespace BVM
{
    int RemoveNptHook(int32_t tag)
    {
        svm_vmmcall(VMMCALL_ID::remove_npt_hook, tag);
        return 0;
    }

    int SetNptHook(uintptr_t address, uint8_t* patch, size_t patch_len, int32_t noexecute_cr3_id, int32_t tag)
    {
        svm_vmmcall(VMMCALL_ID::set_npt_hook, address, patch, patch_len, noexecute_cr3_id, tag);

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