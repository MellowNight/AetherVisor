#include "db_exception.h"
#include "npt_sandbox.h"
#include "branch_tracer.h"

void HandleDebugException(VcpuData* vcpu_data, GeneralRegisters* guest_ctx)
{
    auto vmroot_cr3 = __readcr3();

    __writecr3(vcpu_data->guest_vmcb.save_state_area.Cr3);

    auto guest_rip = vcpu_data->guest_vmcb.save_state_area.Rip;

    //  refactor this into branch_tracer.HandleDebugException and
    //  sandbox.HandleDebugException

    DR6 dr6;
    dr6.Flags = vcpu_data->guest_vmcb.save_state_area.Dr6;

    if (branch_tracer.initialized && guest_rip == branch_tracer.start_address)
    {
        /*  capture the TR of the target thread */

        DbgPrint("vcpu_data->guest_vmcb.save_state_area.TrBase \n", vcpu_data->guest_vmcb.save_state_area.TrBase);

       //  branch_tracer.tr_base = vcpu_data->guest_vmcb.save_state_area.TrBase;

        return;
    }

    if (dr6.SingleInstruction == 1) 
    {
        if (branch_tracer.active && vcpu_data->guest_vmcb.save_state_area.DbgCtl & (1 << IA32_DEBUGCTL_BTF_BIT))
        {
            auto lbr_stack = &vcpu_data->guest_vmcb.save_state_area.lbr_stack[0];

            DbgPrint("LogBranch() \n");
         //   branch_tracer.LogBranch(lbr_stack);
        }

        if (vcpu_data->guest_vmcb.control_area.InterceptVec3 & (~((uint32_t)1 << INTERCEPT_WRITECR3_SHIFT)))
        {
            DbgPrint("re-enabling TR write intercept %p \n", vcpu_data->guest_vmcb.save_state_area.Rip);
            DbgPrint("vcpu_data->guest_vmcb.save_state_area.TrBase \n", vcpu_data->guest_vmcb.save_state_area.TrBase);

            /*  re-enable INTERCEPT_WRITECR3  */

            vcpu_data->guest_vmcb.control_area.InterceptVec3 |= (1UL << INTERCEPT_WRITECR3_SHIFT);

            if (vcpu_data->guest_vmcb.save_state_area.TrBase == branch_tracer.last_branch)
            {
                branch_tracer.Start(vcpu_data);
            }
            else
            {
                branch_tracer.Stop(vcpu_data);
            }
        }

        vcpu_data->guest_vmcb.save_state_area.Rflags &= (~((uint64_t)1 << RFLAGS_TRAP_FLAG_BIT));

        /*	single-step the sandboxed read/write in the ncr3 that allows all pages to be executable	*/

        if (vcpu_data->guest_vmcb.control_area.NCr3 == Hypervisor::Get()->ncr3_dirs[sandbox_single_step])
        {
            DbgPrint("Finished single stepping %p \n", vcpu_data->guest_vmcb.save_state_area.Rip);

            Sandbox::InstructionInstrumentation(vcpu_data, guest_rip, guest_ctx, Sandbox::readwrite_handler, true);
        }
    }
    else      
    {
        InjectException(vcpu_data, EXCEPTION_VECTOR::Debug, FALSE, 0);
    }

    __writecr3(vmroot_cr3);
}
