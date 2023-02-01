#include "vmexit.h"
#include "npt_sandbox.h"
#include "branch_tracer.h"
#include "npt_hook.h"
#include "instrumentation_hook.h"

/*  VmmcallHandler only handles the vmmcall for 1 core.
    It is the guest's responsibility to set thread affinity.

    Parameters are passed in the order of rdx, r8, r9, r12, r11
*/
void VmmcallHandler(VcpuData* vcpu, GuestRegs* guest_ctx, bool* EndVM)
{
    auto id = guest_ctx->rcx;

    switch (id)
    {
    case VMMCALL_ID::start_branch_trace:
    {				
        DbgPrint("VMMCALL_ID::start_branch_trace \n");

        BranchTracer::Init(vcpu, guest_ctx->rdx, guest_ctx->r8, guest_ctx->r9, guest_ctx->r12, guest_ctx->r11);

        break;
    }
    case VMMCALL_ID::deny_sandbox_reads:
    {
        Sandbox::DenyMemoryAccess(vcpu, (void*)guest_ctx->rdx, guest_ctx->r8);

        break;
    }
    case VMMCALL_ID::register_instrumentation_hook:
    {
        auto handler_id = guest_ctx->rdx;

        if (handler_id < Instrumentation::max_id)
        {
            Instrumentation::callbacks[handler_id] = (void*)guest_ctx->r8;
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
        Sandbox::AddPageToSandbox(vcpu, (void*)guest_ctx->rdx, guest_ctx->r8);

        break;
    }
    case VMMCALL_ID::remove_npt_hook:
    {
        auto vmroot_cr3 = __readcr3();

        __writecr3(vcpu->guest_vmcb.save_state_area.cr3.Flags);

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

        __writecr3(vmroot_cr3);

        break;
    }
    case VMMCALL_ID::set_npt_hook:
    {			
        NptHooks::SetNptHook(vcpu, (void*)guest_ctx->rdx, (uint8_t*)guest_ctx->r8, 
            guest_ctx->r9, guest_ctx->r12);

        break;
    }
    case VMMCALL_ID::disable_hv:
    {
        Logger::Get()->Log("[AMD-Hypervisor] - disable_hv vmmcall id %p \n", id);

        *EndVM = true;
        break;
    }
    case VMMCALL_ID::is_hv_present:
    {
        break;
    }
    default:
    {
        InjectException(vcpu, EXCEPTION_INVALID_OPCODE, false, 0);
        return;
    }
    }

    vcpu->guest_vmcb.save_state_area.rip = vcpu->guest_vmcb.control_area.nrip;
}