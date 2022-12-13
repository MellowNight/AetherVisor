#include "vmexit.h"
#include "npt_sandbox.h"
#include "branch_tracer.h"
#include "npt_hook.h"

/*  HandleVmmcall only handles the vmmcall for 1 core.
    It is the guest's responsibility to set thread affinity.

    Parameters are passed in the order of rdx, r8, r9, r12, r11
*/
void HandleVmmcall(VcpuData* vcpu_data, GuestRegisters* guest_ctx, bool* EndVM)
{
    auto id = guest_ctx->rcx;

    switch (id)
    {
    case VMMCALL_ID::start_branch_trace:
    {				
        DbgPrint("VMMCALL_ID::start_branch_trace \n");

        BranchTracer::Init(vcpu_data, guest_ctx->rdx, guest_ctx->r8);

        break;
    }
    case VMMCALL_ID::deny_sandbox_reads:
    {
        Sandbox::DenyMemoryAccess(vcpu_data, (void*)guest_ctx->rdx);

        break;
    }
    case VMMCALL_ID::register_sandbox:
    {
        auto handler_id = guest_ctx->rdx;

        if (handler_id == Sandbox::readwrite_handler || handler_id == Sandbox::execute_handler)
        {
            Sandbox::sandbox_hooks[handler_id] = (void*)guest_ctx->r8;
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
        Sandbox::AddPageToSandbox(vcpu_data, (void*)guest_ctx->rdx, guest_ctx->r8);

        break;
    }
    case VMMCALL_ID::remove_npt_hook:
    {
        auto vmroot_cr3 = __readcr3();

        __writecr3(vcpu_data->guest_vmcb.save_state_area.Cr3.Flags);

        NPTHooks::ForEachHook(
            [](auto hook_entry, auto data)-> auto {

                if (hook_entry->address == data)
                {
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
        NPTHooks::SetNptHook(vcpu_data, (void*)guest_ctx->rdx, (uint8_t*)guest_ctx->r8, 
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
        InjectException(vcpu_data, EXCEPTION_INVALID_OPCODE, false, 0);
        return;
    }
    }

    vcpu_data->guest_vmcb.save_state_area.Rip = vcpu_data->guest_vmcb.control_area.NRip;
}