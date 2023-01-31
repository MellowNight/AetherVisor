#include "vmexit.h"
#include "npt_sandbox.h"
#include "branch_tracer.h"
#include "instrumentation_hook.h"

void HandleDebugException(VcpuData* vcpu_data, GuestRegisters* guest_ctx)
{
    auto guest_rip = vcpu_data->guest_vmcb.save_state_area.rip;

    DR6 dr6 = vcpu_data->guest_vmcb.save_state_area.Dr6;
    
    if (dr6.SingleInstruction == 1) 
    {
        if (BranchTracer::active == true && 
            (vcpu_data->guest_vmcb.save_state_area.Dr7.Flags & ((uint64_t)1 << 9)) && 
            vcpu_data->guest_vmcb.save_state_area.cr3.Flags == BranchTracer::process_cr3.Flags)
        {
            if (guest_rip < BranchTracer::range_base || guest_rip > (BranchTracer::range_size + BranchTracer::range_base))
            {
                /*  Pause branch tracer after a branch outside of the specified range is executed.
                    Single-stepping mode => completely disabled 
                */

                BranchTracer::Pause(vcpu_data);
            }

            auto vmroot_cr3 = __readcr3();

            __writecr3(vcpu_data->guest_vmcb.save_state_area.cr3.Flags);

            DbgPrint("LastBranchFromIP %p guest_rip = %p \n", vcpu_data->guest_vmcb.save_state_area.br_from, guest_rip);

            BranchTracer::log_buffer->Log(vcpu_data, guest_rip, vcpu_data->guest_vmcb.save_state_area.br_from);

            __writecr3(vmroot_cr3);

            if (guest_rip == BranchTracer::stop_address)
            {
                BranchTracer::Stop(vcpu_data);

                Instrumentation::InvokeHook(vcpu_data, Instrumentation::branch_trace_finished, false);
            }

            return;
        }

        /*  Transfer RIP to the instrumentation hook for read instructions.
            Single-stepping mode => completely disabled
        */

        if (vcpu_data->guest_vmcb.control_area.ncr3 == Hypervisor::Get()->ncr3_dirs[sandbox_single_step])
        {
            BranchTracer::Pause(vcpu_data);

            DbgPrint("Finished single stepping %p \n", vcpu_data->guest_vmcb.save_state_area.rip);

            Instrumentation::InvokeHook(vcpu_data, Instrumentation::sandbox_readwrite, false);
        }
    }
    else      
    {
        InjectException(vcpu_data, EXCEPTION_VECTOR::Debug, FALSE, 0);
    }
}
