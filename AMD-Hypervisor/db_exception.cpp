#include "vmexit.h"
#include "npt_sandbox.h"
#include "branch_tracer.h"

void HandleDebugException(VcpuData* vcpu_data, GuestRegisters* guest_ctx)
{
    auto guest_rip = vcpu_data->guest_vmcb.save_state_area.Rip;

    DR6 dr6 = vcpu_data->guest_vmcb.save_state_area.Dr6;

    if (dr6.SingleInstruction == 1) 
    {
        if (BranchTracer::active == true && vcpu_data->guest_vmcb.save_state_area.DbgCtl.Btf)
        {
            DbgPrint("branch address = %p \n", guest_rip);

            BranchTracer::log_buffer->Log(guest_rip);

            return;
        }

        vcpu_data->guest_vmcb.save_state_area.Rflags.TrapFlag = 0;
        vcpu_data->guest_vmcb.save_state_area.DbgCtl.Btf = 1;

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
}
