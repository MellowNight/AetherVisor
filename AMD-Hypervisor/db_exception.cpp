#include "vmexit.h"
#include "npt_sandbox.h"
#include "branch_tracer.h"

void HandleDebugException(VcpuData* vcpu_data, GuestRegisters* guest_ctx)
{
    auto guest_rip = vcpu_data->guest_vmcb.save_state_area.Rip;

    DR6 dr6 = vcpu_data->guest_vmcb.save_state_area.Dr6;
    
    if (dr6.SingleInstruction == 1) 
    {
        if (BranchTracer::active == true && (vcpu_data->guest_vmcb.save_state_area.Dr7.Flags & (1 << 9)))
        {
            if (guest_rip > BranchTracer::range_base && guest_rip < BranchTracer::range_size + BranchTracer::range_base)
            {
                /*  Pause branch tracer after a branch outside of the specified range is executed.
                    Single-stepping mode => completely disabled 
                */

                BranchTracer::Pause(vcpu_data);
            }

            auto vmroot_cr3 = __readcr3();

            __writecr3(vcpu_data->guest_vmcb.save_state_area.Cr3.Flags);

            DbgPrint("LastBranchToIp LastBranchFromIp = %p \n", guest_rip);

            BranchTracer::log_buffer->Log(guest_rip);

            __writecr3(vmroot_cr3);

            return;
        }

        /*  Transfer RIP to the read instruction instrumentation hook.
        	Single-stepping mode => completely disabled 
        */

        if (vcpu_data->guest_vmcb.control_area.NCr3 == Hypervisor::Get()->ncr3_dirs[sandbox_single_step])
        {
            BranchTracer::Pause(vcpu_data);

            DbgPrint("Finished single stepping %p \n", vcpu_data->guest_vmcb.save_state_area.Rip);

            Sandbox::InstructionInstrumentation(vcpu_data, guest_rip, guest_ctx, Sandbox::readwrite_handler, false);
        }
    }
    else      
    {
        InjectException(vcpu_data, EXCEPTION_VECTOR::Debug, FALSE, 0);
    }
}
