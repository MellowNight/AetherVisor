#include "aethervisor.h"
#include "ia32.h"
#include "utils.h"

/*  vmmcall parameter order: rcx, rdx, r8, r9, r12, r11  */

namespace AetherVisor
{
    BranchLog* log_buffer;

    void TraceFunction(uint8_t* start_addr, uintptr_t range_base, uintptr_t range_size, uint8_t* stop_addr = NULL)
    {
        log_buffer = new BranchLog{};

        SetNptHook((uintptr_t)start_addr, (uint8_t*)"\xCC", 1, sandbox);
        
        svm_vmmcall(VMMCALL_ID::start_branch_trace, start_addr, stop_addr, log_buffer, range_base, range_size);
    }

    struct Hook
    {
        HOOK_ID id;
        void** handler;
        void (*handler_wrap)();
    };

    Hook instrumentation_hooks[] = {
        // Invoked when sandboxed code reads/writes from a page that denies read/write access.
        {sandbox_readwrite, (void**)&sandbox_mem_access_handler, rw_handler_wrap}, 

        // Invoked every time RIP leaves a sandbox memory region
        {sandbox_execute, (void**)&sandbox_execute_handler, execute_handler_wrap},

        // Invoked when branch trace buffer is full
        {branch_log_full, (void**)&branch_log_full_handler, branch_log_full_handler_wrap},

        // Invoked when the branch tracer has reached the stop address
        {branch_trace_finished, (void**)&branch_trace_finish_handler, branch_trace_finish_handler_wrap}
    };

    void InstrumentationHook(HOOK_ID handler_id, void* address)
    {
        for (const auto& hook : instrumentation_hooks) 
        {
            if (hook.id == handler_id) 
            {
                *hook.handler = static_cast<decltype(*hook.handler)>(address);
                svm_vmmcall(VMMCALL_ID::instrumentation_hook, handler_id, hook.handler_wrap);
                break;
            }
        }
    }

    void HookEferSyscalls()
    {
        Util::ForEachCore(
            [](void* params) -> void {
                svm_vmmcall(VMMCALL_ID::hook_efer_syscall);
            }, NULL
        );
    }

    void DenySandboxMemAccess(void* page_addr, bool allow_reads)
    {
        svm_vmmcall(VMMCALL_ID::deny_sandbox_reads, page_addr);
    }

#pragma optimize( "", off )

    int SetNptHook(uintptr_t address, uint8_t* patch, size_t patch_len, int32_t ncr3_id, bool global_page)
    {
        Util::TriggerCOW((uint8_t*)address);

        svm_vmmcall(VMMCALL_ID::set_npt_hook, address, patch, patch_len, ncr3_id);
    }
#pragma optimize( "", on )

    int SandboxPage(uintptr_t address, uintptr_t tag)
    {
        Util::TriggerCOW((uint8_t*)address);

        svm_vmmcall(VMMCALL_ID::sandbox_page, address, tag);

        return 0;
    }

    void SandboxRegion(uintptr_t base, uintptr_t size)
    {
        for (auto offset = base; offset < base + size; offset += PAGE_SIZE)
        {
            AetherVisor::SandboxPage((uintptr_t)offset, NULL);
        }
    }

    int RemoveNptHook(uintptr_t address)
    {
        svm_vmmcall(VMMCALL_ID::remove_npt_hook, address);

        return 0;
    }

  
    int DisableHv()
    {
        Util::ForEachCore(
            [](void* params) -> void 
            {
                svm_vmmcall(VMMCALL_ID::disable_hv);
            }, NULL
        );
        return 0;
    }
};