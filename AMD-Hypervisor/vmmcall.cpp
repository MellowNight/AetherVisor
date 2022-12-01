#include "vmexit.h"
#include "npt_sandbox.h"
#include "branch_tracer.h"
#include "npt_hook.h"

/*  HandleVmmcall only handles the vmmcall for 1 core.
    It is the guest's responsibility to set thread affinity.
*/
void HandleVmmcall(VcpuData* vcpu_data, GeneralRegisters* GuestRegisters, bool* EndVM)
{
    auto id = GuestRegisters->rcx;

    switch (id)
    {
    case VMMCALL_ID::start_branch_trace:
    {
        BranchTracer::StartTrace(vcpu_data, (void*)GuestRegisters->rcx, (int)GuestRegisters->rdx);
        
        break;
    }
    case VMMCALL_ID::deny_sandbox_reads:
    {
        Sandbox::DenyMemoryAccess(vcpu_data, (void*)GuestRegisters->rdx);
        break;
    }
    case VMMCALL_ID::register_sandbox:
    {
        auto handler_id = GuestRegisters->rdx;

        if (handler_id == Sandbox::readwrite_handler || handler_id == Sandbox::execute_handler)
        {
            Sandbox::sandbox_hooks[handler_id] = (void*)GuestRegisters->r8;
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
        Sandbox::AddPageToSandbox(vcpu_data, (void*)GuestRegisters->rdx, GuestRegisters->r8);

        break;
    }
    case VMMCALL_ID::remove_npt_hook:
    {
        auto vmroot_cr3 = __readcr3();

        __writecr3(vcpu_data->guest_vmcb.save_state_area.Cr3);

        NPTHooks::ForEachHook(
            [](NPTHooks::NptHook* hook_entry, void* data)-> bool {

                if (hook_entry->tag == (uintptr_t)data)
                {
                    NPTHooks::UnsetHook(hook_entry);
                }
                return true;
            },
            (void*)GuestRegisters->rdx
        );

        __writecr3(vmroot_cr3);

        break;
    }
    case VMMCALL_ID::set_npt_hook:
    {			
        NPTHooks::SetNptHook(
            vcpu_data, 
            (void*)GuestRegisters->rdx, 
            (uint8_t*)GuestRegisters->r8,
            GuestRegisters->r9, 
            GuestRegisters->r12, 
            GuestRegisters->r11
        );

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