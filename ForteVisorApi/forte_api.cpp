#include "pch.h"
#include "forte_api.h"

namespace ForteVisor
{
    /*  Not on each core, because it's only relevant in 1 process context */
    int SetTlbHook(uintptr_t address, uint8_t* patch, size_t patch_len)
    {
        return svm_vmmcall(VMMCALL_ID::set_tlb_hook, address, patch, patch_len);
    }

    int SetNptHook(uintptr_t address, uint8_t* patch, size_t patch_len)
    {
        svm_vmmcall(
            VMMCALL_ID::set_npt_hook,
            address,
            patch,
            patch_len
        );
        return 0;
    }

    int SetMpkHook(uintptr_t address, uint8_t* patch, size_t patch_len)
    {
        struct HookInfo
        {
            uintptr_t address;
            uint8_t* patch;
            size_t patch_len;
        } hook_info;

        hook_info = { address, patch, patch_len };

        ForEachCore(
            [](void* params) -> void
            {
                auto hook_info = (HookInfo*)params;

                svm_vmmcall(VMMCALL_ID::set_mpk_hook, hook_info->address, hook_info->patch, hook_info->patch_len);
            },
            (void*)&hook_info
        );

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