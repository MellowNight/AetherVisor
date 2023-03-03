
#include "aethervisor.h"
#include "utils.h"

void (*sandbox_execute_event)(GuestRegisters* registers, void* return_address, void* o_guest_rip);

void (*sandbox_mem_access_event)(GuestRegisters* registers, void* o_guest_rip);

void (*branch_callback)(GuestRegisters* registers, void* return_address, void* o_guest_rip, void* LastBranchFromIP);

void (*branch_trace_finish_event)();

void (*syscall_hook)(GuestRegisters* registers, void* return_address, void* o_guest_rip);

/*  vmmcall parameter order: rcx, rdx, r8, r9, r12, r11  */

namespace Aether
{
    Callback instrumentation_hooks[] = {
        // Invoked when sandboxed code reads/writes from a page that denies read/write access.
        {sandbox_readwrite, (void**)&sandbox_mem_access_event, rw_handler_wrapper, NULL},

        // Invoked every time RIP leaves a sandbox memory region
        {sandbox_execute, (void**)&sandbox_execute_event, execute_handler_wrapper, NULL},

        // Invoked when branch trace buffer is full
        {branch, (void**)&branch_callback, branch_callback_wrapper, NULL},

        // Invoked when the branch tracer has reached the stop address
        {branch_trace_finished, (void**)&branch_trace_finish_event, branch_trace_finish_event_wrap, NULL},

        // EFER MSR Syscall hook handler
        {syscall, (void**)&syscall_hook, syscall_hook_wrap, NULL}
    };

    void SetCallback(CALLBACK_ID handler_id, void* address)
    {
        *instrumentation_hooks[handler_id].handler = address;

        svm_vmmcall(VMMCALL_ID::instrumentation_hook, 
            handler_id, *instrumentation_hooks[handler_id].handler_wrapper, instrumentation_hooks[handler_id].tls_params_idx);
    }

    int _cdecl StopHv()
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