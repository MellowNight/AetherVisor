#include "pch.h"
#include "forte_api.h"
#include "ia32.h"
#include "utils.h"

void (*sandbox_execute_handler)(GuestRegs* registers, void* return_address, void* o_guest_rip) = NULL;
void (*sandbox_mem_access_handler)(GuestRegs* registers, void* o_guest_rip) = NULL;
void (*branch_log_full_handler)() = NULL;
void (*branch_trace_finish_handler)() = NULL;
void (*syscall_handler)() = NULL;

/*  parameter order: rcx, rdx, r8, r9, r12, r11  */

namespace BVM
{
    BranchLog* log_buffer;

    void TraceFunction(uint8_t* start_addr, uintptr_t range_base, uintptr_t range_size)
    {
        log_buffer = new BranchLog{};

        SetNptHook((uintptr_t)start_addr, (uint8_t*)"\xCC", 1, sandbox);

        svm_vmmcall(VMMCALL_ID::start_branch_trace, start_addr, NULL, log_buffer, range_base, range_size);
    }

    void InstrumentationHook(HookId handler_id, void* address)
    {
        switch (handler_id)
        {
            case sandbox_readwrite:
            {
                sandbox_mem_access_handler = static_cast<decltype(sandbox_mem_access_handler)>(address);
                svm_vmmcall(VMMCALL_ID::register_instrumentation_hook, handler_id, rw_handler_wrap);
                break;
            }
            case sandbox_execute:
            {
                sandbox_execute_handler = static_cast<decltype(sandbox_execute_handler)>(address);
                svm_vmmcall(VMMCALL_ID::register_instrumentation_hook, handler_id, execute_handler_wrap);                break;
                break;
            }
            case branch_log_full:
            {
                branch_log_full_handler = static_cast<decltype(branch_log_full_handler)>(address);
                svm_vmmcall(VMMCALL_ID::register_instrumentation_hook, handler_id, branch_log_full_handler_wrap);
                break;
            }
            case branch_trace_finished:
            {
                branch_trace_finish_handler = static_cast<decltype(branch_trace_finish_handler)>(address);
                svm_vmmcall(VMMCALL_ID::register_instrumentation_hook, handler_id, branch_trace_finish_handler_wrap);
                break;
            }
        }
    }

    void DenySandboxMemAccess(void* page_addr)
    {
        svm_vmmcall(VMMCALL_ID::deny_sandbox_reads, page_addr);
    }

#pragma optimize( "", off )

    int SetNptHook(uintptr_t address, uint8_t* patch, size_t patch_len, int32_t ncr3_id, bool global_page)
    {
        int a;

        if (global_page)
        {
            Util::TriggerCOWAndPageIn((uint8_t*)address);
        }
        else
        {
            /*  page in */

            memcpy(&a, (void*)address, sizeof(int));
        }

        svm_vmmcall(VMMCALL_ID::set_npt_hook, address, patch, patch_len, ncr3_id);

        return a;
    }
#pragma optimize( "", on )

    int SandboxPage(uintptr_t address, uintptr_t tag)
    {
        Util::TriggerCOWAndPageIn((uint8_t*)address);

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

  
    int DisableHv()
    {
        Util::ForEachCore(
            [](void* params) -> void {
                svm_vmmcall(VMMCALL_ID::disable_hv);
            }, NULL
        );
        return 0;
    }
};