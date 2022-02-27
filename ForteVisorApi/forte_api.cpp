#include "pch.h"
#include "forte_api.h"

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

    int SetNptHook(uintptr_t address, uint8_t* patch, size_t patch_len)
    {
        struct HookParams
        {
            uintptr_t address;
            uint8_t* patch;
            size_t patch_len;
        };

        HookParams hook_param = HookParams{ address, patch, patch_len };

        ForEachCore(
            [](void* param) -> void {

                auto hook = (HookParams*)param;

                vmmcall(
                    VMMCALL_ID::set_npt_hook,
                    hook->address,
                    hook->patch,
                    hook->patch_len
                );
            },
            (void*)&hook_param
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
                vmmcall(VMMCALL_ID::disable_hv);
            }
        );
        return 0;
    }
};