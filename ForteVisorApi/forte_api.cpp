#include "pch.h"
#include "forte_api.h"

void (*sandbox_execute_handler)(GeneralRegisters* registers, void* return_address, void* o_guest_rip) = NULL;
void (*sandbox_mem_access_handler)(GeneralRegisters* registers, void* o_guest_rip) = NULL;

/*  parameter order: rcx, rdx, r8, r9, r12, r11  */

namespace ForteVisor
{
    void SandboxMemAccessHandler(GeneralRegisters* registers, void* o_guest_rip)
    {
        sandbox_mem_access_handler(registers, o_guest_rip);
    }

    void SandboxExecuteHandler(GeneralRegisters* registers, void* return_address, void* o_guest_rip)
    {
        sandbox_execute_handler(registers, return_address, o_guest_rip);
    }

    void RegisterSandboxHandler(SandboxHookId handler_id, void* address)
    {
        if (handler_id == readwrite_handler)
        {
            sandbox_rw_handler = static_cast<decltype(sandbox_rw_handler)>(address);
            svm_vmmcall(VMMCALL_ID::register_sandbox, handler_id, rw_handler_wrap);
        }
        else if (handler_id == execute_handler)
        {
            sandbox_execute_handler = static_cast<decltype(sandbox_execute_handler)>(address);
            svm_vmmcall(VMMCALL_ID::register_sandbox, handler_id, execute_handler_wrap);
        }
    }

    int SetNptHook(uintptr_t address, uint8_t* patch, size_t patch_len, int32_t noexecute_cr3_id, uintptr_t tag)
    {
        svm_vmmcall(VMMCALL_ID::set_npt_hook, address, patch, patch_len, noexecute_cr3_id, tag);

        return 0;
    }

    int SandboxPage(uintptr_t address, uintptr_t tag)
    {
        svm_vmmcall(VMMCALL_ID::sandbox_page, address, tag);

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