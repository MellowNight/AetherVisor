#include "pch.h"
#include "forte_api.h"

void (*sandbox_execute_handler)(GeneralRegisters* registers, void* return_address, void* o_guest_rip) = NULL;
void (*sandbox_mem_access_handler)(GeneralRegisters* registers, void* o_guest_rip) = NULL;

/*  parameter order: rcx, rdx, r8, r9, r12, r11  */

namespace ForteVisor
{
    LogBuffer* log_buffer;

    void TraceFunction(uint8_t* start_addr)
    {
        auto log_size = 0x1000;

        log_buffer = (LogBuffer*)VirtualAlloc(NULL, log_size, MEM_COMMIT, PAGE_READWRITE);

        svm_vmmcall(VMMCALL_ID::start_branch_trace, start_addr, log_buffer, log_size);

        /*  throw #DB to capture task register  */

        DR7 dr7;
        dr7.Flags = __readdr(7);

        dr7.GlobalBreakpoint0 = 1;
        dr7.Length0 = 0;
        dr7.ReadWrite0 = 0;

        __writedr(7, dr7.Flags);
    }    

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
            sandbox_mem_access_handler = static_cast<decltype(sandbox_mem_access_handler)>(address);
            svm_vmmcall(VMMCALL_ID::register_sandbox, handler_id, rw_handler_wrap);
        }
        else if (handler_id == execute_handler)
        {
            sandbox_execute_handler = static_cast<decltype(sandbox_execute_handler)>(address);
            svm_vmmcall(VMMCALL_ID::register_sandbox, handler_id, execute_handler_wrap);
        }
    }

    void DenySandboxMemAccess(void* page_addr)
    {
        svm_vmmcall(VMMCALL_ID::deny_sandbox_reads, page_addr);
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