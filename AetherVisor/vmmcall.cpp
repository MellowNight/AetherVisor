#include "vmexit.h"
#include "npt_sandbox.h"
#include "branch_tracer.h"
#include "npt_hook.h"
#include "instrumentation_hook.h"
#include "syscall_hook.h"

/*  VmmcallHandler only handles the vmmcall for 1 core.
    It is the guest's responsibility to set thread affinity.

    Parameters are passed in the order of rdx, r8, r9, r12, r11
*/

void VcpuData::VmmcallHandler(GuestRegisters* guest_ctx, bool* end_svm)
{
    auto id = guest_ctx->rcx;

    suppress_nrip_increment = FALSE;

    switch (id)
    {
    case VMMCALL_ID::start_branch_trace:
    {
        BranchTracer::Init(this, guest_ctx->rdx, guest_ctx->r8, guest_ctx->r9, guest_ctx->r12);

        break;
    }
    case VMMCALL_ID::stop_branch_trace:
    {
        BranchTracer::Stop(this);

        break;
    }
    case VMMCALL_ID::deny_sandbox_reads:
    {
        Sandbox::DenyMemoryAccess(this, (void*)guest_ctx->rdx, guest_ctx->r8);

        break;
    }
    case VMMCALL_ID::instrumentation_hook:
    {
        auto handler_id = guest_ctx->rdx;
        auto tls_idx = guest_ctx->r9;
        auto function = guest_ctx->r8;

        if (handler_id < Instrumentation::max_id)
        {
            Instrumentation::callbacks[handler_id] = { (void*)function, (uint32_t)tls_idx };
        }
        else
        {
            /*  invalid input   */
            __debugbreak();
        }

        break;
    }
    case VMMCALL_ID::sandbox_page:
    {
        /*  exclude calls/jmps to IDT    */

        Sandbox::branch_exclusion_range_base = guest_ctx->r9;

        Sandbox::AddPageToSandbox(this, (void*)guest_ctx->rdx, guest_ctx->r8);

        break;
    }
    case VMMCALL_ID::unbox_page:
    {
        DbgPrint("unbox_page virtual address %p \n", guest_ctx->rdx);

        Sandbox::ForEachHook(
            [](auto hook_entry, auto data) -> auto {

                DbgPrint("PAGE_ALIGN(hook_entry->guest_physical) %p  data %p \n", PAGE_ALIGN(hook_entry->guest_physical), data);

                if (PAGE_ALIGN(hook_entry->guest_physical) == data)
                {
                    DbgPrint("Releasing sandbox page %p \n", hook_entry->guest_physical);
                    Sandbox::ReleasePage(hook_entry);
                }

                return false;
            },
            (void*)PAGE_ALIGN(MmGetPhysicalAddress((void*)guest_ctx->rdx).QuadPart)
        );


        break;
    }
    case VMMCALL_ID::remove_npt_hook:
    {
        DbgPrint("VMMCALL_ID::remove_npt_hook called! \n");

        NptHooks::ForEachHook(
            [](auto hook_entry, auto data)-> auto {

                if (hook_entry->address == data)
                {
                    DbgPrint("unsetting NPT hook at %p \n", hook_entry->address);
                    UnsetHook(hook_entry);
                }

                return false;
            },
            (void*)guest_ctx->rdx
        );

        break;
    }
    case VMMCALL_ID::set_npt_hook:
    {
        NptHooks::SetNptHook(this, (void*)guest_ctx->rdx, 
            (uint8_t*)guest_ctx->r8, guest_ctx->r9, guest_ctx->r12);

        break;
    }
    case VMMCALL_ID::disable_hv:
    {
        Logger::Get()->Log("[AMD-Hypervisor] - disable_hv vmmcall id %p \n", id);

        *end_svm = true;
        break;
    }
    case VMMCALL_ID::is_hv_present:
    {
        guest_vmcb.save_state_area.rax = TRUE;

        break;
    }
    case VMMCALL_ID::hook_efer_syscall:
    {
        SyscallHook::Toggle(this, guest_ctx->rdx);

        break;
    }
    default:
    {
        InjectException(EXCEPTION_VECTOR::InvalidOpcode, false, 0);
        return;
    }
    }

    if (suppress_nrip_increment == FALSE)
    {
        guest_vmcb.save_state_area.rip = guest_vmcb.control_area.nrip;
    }
}