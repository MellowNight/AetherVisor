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

    int SetNptHook(uintptr_t address, uint8_t* patch, size_t patch_len)
    {
        struct HookParams
        {
            uintptr_t address, 
            uint8_t* patch,
            size_t patch_len
        };

        HookParams hook_param = HookParams{address, patch, patch_len};

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

    int Exponent(int base, int power)
    {
        int start = 1;
        for (int i = 0; i < power; ++i)
        {
            start *= base;
        }

        return start;
    }

    int ForEachCore(void(*callback)(void* params), void* params = NULL)
    {
	    auto core_count = KeQueryActiveProcessorCount(0);

        for (int idx = 0; idx < core_count; ++idx)
        {
            KAFFINITY affinity = Exponent(2, idx);

            KeSetSystemAffinityThread(affinity);
            auto affinity = pow(2, idx);
            
            SetThreadAffinityMask(GetCurrentThread(), affinity);

            callback(params);
        }

        return 0;
    }

    int DisableHv()
    {
        ForEachCore(
            [](void* param) -> void {
                vmmcall(VMMCALL_ID::disable_hv);
            }
        );
        return 0;
    }
};