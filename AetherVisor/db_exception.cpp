#include "vmexit.h"
#include "npt_sandbox.h"
#include "branch_tracer.h"
#include "instrumentation_hook.h"


void VcpuData::DebugFaultHandler(GuestRegisters* guest_ctx)
{
    auto guest_rip = guest_vmcb.save_state_area.rip;

    DR6 dr6 = guest_vmcb.save_state_area.dr6;

    if (dr6.SingleInstruction == 1)
    {
        if (BranchTracer::active == true &&
            (guest_vmcb.save_state_area.dr7.Flags & ((uint64_t)1 << 9)) &&
            guest_vmcb.save_state_area.cr3.Flags == BranchTracer::process_cr3.Flags)
        {
            if (guest_rip < BranchTracer::range_base || guest_rip > (BranchTracer::range_size + BranchTracer::range_base))
            {
                /*  Pause branch tracer after a branch outside of the specified range is executed.
                    Single-stepping mode => completely disabled
                */

                BranchTracer::Pause(this);
            }

            auto vmroot_cr3 = __readcr3();

            __writecr3(guest_vmcb.save_state_area.cr3.Flags);

            DbgPrint("LastBranchFromIP %p guest_rip = %p \n", guest_vmcb.save_state_area.br_from, guest_rip);

            BranchTracer::log_buffer->Log(this, guest_rip, guest_vmcb.save_state_area.br_from);

            __writecr3(vmroot_cr3);

            if (guest_rip == BranchTracer::stop_address)
            {
                BranchTracer::Stop(this);

                Instrumentation::InvokeHook(this, Instrumentation::branch_trace_finished, false);
            }

            return;
        }

        /*  Transfer RIP to the instrumentation hook for read instructions.
            Single-stepping mode => completely disabled
        */

        if (guest_vmcb.control_area.ncr3 == Hypervisor::Get()->ncr3_dirs[sandbox_single_step])
        {
            BranchTracer::Pause(this);

            DbgPrint("Finished single stepping %p \n", guest_vmcb.save_state_area.rip);

            Instrumentation::InvokeHook(this, Instrumentation::sandbox_readwrite, false);
        }
    }
    else
    {
        InjectException(EXCEPTION_VECTOR::Debug, FALSE, 0);
    }
}