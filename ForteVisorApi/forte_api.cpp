#include "pch.h"
#include "forte_api.h"
#include "ia32.h"

void (*sandbox_execute_handler)(GuestRegisters* registers, void* return_address, void* o_guest_rip) = NULL;
void (*sandbox_mem_access_handler)(GuestRegisters* registers, void* o_guest_rip) = NULL;

/*  parameter order: rcx, rdx, r8, r9, r12, r11  */

namespace BVM
{
    BranchLog* log_buffer;

    void WriteToReadOnly(void* address, uint8_t* bytes, size_t len)
    {
        DWORD old_prot;
        VirtualProtect((LPVOID)address, len, PAGE_EXECUTE_READWRITE, &old_prot);
        memcpy((void*)address, (void*)bytes, len);
        VirtualProtect((LPVOID)address, len, old_prot, 0);
    }

    void TriggerCOWAndPageIn(void* address)
    {
        uint8_t buffer;

        /*	1. page in	*/

        buffer = *(uint8_t*)address;

        /*	2. trigger COW	*/

        WriteToReadOnly(address, (uint8_t*)"\xC3", 1);
        WriteToReadOnly(address, &buffer, 1);
    }

    void TraceFunction(uint8_t* start_addr)
    {
        auto log_size = 0x1000;

        log_buffer = new BranchLog{ log_size };

        SetNptHook((uintptr_t)start_addr, (uint8_t*)"\xCC", 1, sandbox);

        svm_vmmcall(VMMCALL_ID::start_branch_trace, start_addr, log_buffer);
    }

    void SandboxMemAccessHandler(GuestRegisters* registers, void* o_guest_rip)
    {
        sandbox_mem_access_handler(registers, o_guest_rip);
    }

    void SandboxExecuteHandler(GuestRegisters* registers, void* return_address, void* o_guest_rip)
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

    int SetNptHook(uintptr_t address, uint8_t* patch, size_t patch_len, int32_t ncr3_id)
    {
        TriggerCOWAndPageIn((uint8_t*)address);

        svm_vmmcall(VMMCALL_ID::set_npt_hook, address, patch, patch_len, ncr3_id);

        return 0;
    }

    int SandboxPage(uintptr_t address, uintptr_t tag)
    {
        TriggerCOWAndPageIn((uint8_t*)address);

        svm_vmmcall(VMMCALL_ID::sandbox_page, address, tag);

        return 0;
    }

    void SandboxRegion(uintptr_t base, uintptr_t size)
    {
        for (auto offset = base; offset < base + size; offset += PAGE_SIZE)
        {
            BVM::SandboxPage((uintptr_t)offset, NULL);
        }
    }

    int RemoveNptHook(uintptr_t address)
    {
        svm_vmmcall(VMMCALL_ID::remove_npt_hook, address);

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
            }, NULL
        );
        return 0;
    }
};