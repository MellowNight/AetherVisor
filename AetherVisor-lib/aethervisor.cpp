
#include "aethervisor.h"
#include "utils.h"

void (*sandbox_execute_event)(GuestRegisters* registers, void* return_address, void* o_guest_rip);

void (*sandbox_mem_access_event)(GuestRegisters* registers, void* o_guest_rip);

void (*branch_log_full_event)();

void (*branch_trace_finish_event)();

void (*syscall_hook)(GuestRegisters* registers, void* return_address, void* o_guest_rip);

/*  vmmcall parameter order: rcx, rdx, r8, r9, r12, r11  */

namespace Aether
{
    struct Callback
    {
        CALLBACK_ID id;
        void** handler;
        void (*handler_wrapper)();
    };

    Callback instrumentation_hooks[] = {
        // Invoked when sandboxed code reads/writes from a page that denies read/write access.
        {sandbox_readwrite, (void**)&sandbox_mem_access_event, rw_handler_wrap},

        // Invoked every time RIP leaves a sandbox memory region
        {sandbox_execute, (void**)&sandbox_execute_event, execute_handler_wrap},

        // Invoked when branch trace buffer is full
        {branch_log_full, (void**)&branch_log_full_event, branch_log_full_event_wrap},

        // Invoked when the branch tracer has reached the stop address
        {branch_trace_finished, (void**)&branch_trace_finish_event, branch_trace_finish_event_wrap},

        // EFER MSR Syscall hook handler
        {syscall, (void**)&syscall_hook, syscall_hook_wrap}
    };

    void SetCallback(CALLBACK_ID handler_id, void* address)
    {
        *instrumentation_hooks[handler_id].handler = address;

        svm_vmmcall(VMMCALL_ID::instrumentation_hook, handler_id, *instrumentation_hooks[handler_id].handler_wrapper);
    }

    int StopHv()
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