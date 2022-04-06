#include "pch.h"
#include "forte_api.h"

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

    int RemoveNptHook(int32_t tag)
    {
        svm_vmmcall(VMMCALL_ID::remove_npt_hook, tag);
        return 0;
    }

    int ForEachCore(void(*callback)(void* params), void* params)
    {
        SYSTEM_INFO sys_info;
        GetSystemInfo(&sys_info);
        auto core_count = sys_info.dwNumberOfProcessors;

        for (auto idx = 0; idx < core_count; ++idx)
        {
            auto affinity = pow(2, idx);

            SetThreadAffinityMask(GetCurrentThread(), affinity);

            callback(params);
        }

        return 0;
    }

    int DisableHv()
    {
        ForEachCore(
            [](void* params) -> void {
                svm_vmmcall(VMMCALL_ID::disable_hv);
            }
        );
        return 0;
    }
};