#include "vmexit.h"
#include "npt_sandbox.h"
#include "branch_tracer.h"
#include "instrumentation_hook.h"


void VcpuData::DebugFaultHandler(GuestRegisters* guest_ctx)
{
    auto guest_rip = guest_vmcb.save_state_area.rip;

    DR6 dr6 = guest_vmcb.save_state_area.dr6;

    DbgPrint("[DebugFaultHandler]   dr6.SingleInstruction = %i \n", dr6.SingleInstruction);
    DbgPrint("[DebugFaultHandler]   guest_vmcb.save_state_area.dr7.Flags & ((uint64_t)1 << 9)) = %i \n", guest_vmcb.save_state_area.dr7.Flags & ((uint64_t)1 << 9));
    DbgPrint("[DebugFaultHandler]   BranchTracer::process_cr3.Flags %p \n", BranchTracer::process_cr3.Flags);
    DbgPrint("[DebugFaultHandler]   guest_vmcb.save_state_area.cr3.Flags %p \n", guest_vmcb.save_state_area.cr3.Flags);
    DbgPrint("[DebugFaultHandler]   BranchTracer::range_base %p \n", BranchTracer::range_base);
    DbgPrint("[DebugFaultHandler]   BranchTracer::range_base + BranchTracer::range_size %p \n\n\n", BranchTracer::range_size + BranchTracer::range_base);

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

            DbgPrint("[DebugFaultHandler]   LastBranchFromIP %p guest_rip = %p \n", guest_vmcb.save_state_area.br_from, guest_rip);

            BranchTracer::log_buffer->Log(this, guest_rip, guest_vmcb.save_state_area.br_from);

            if (guest_rip == BranchTracer::stop_address)
            {
                BranchTracer::Stop(this);

                Instrumentation::InvokeHook(this, Instrumentation::branch_trace_finished);
            }

            return;
        }
        else if (guest_vmcb.control_area.ncr3 == Hypervisor::Get()->ncr3_dirs[sandbox_single_step])
        {
            /*  Transfer RIP to the instrumentation hook for read instructions.
                Single-stepping mode => completely disabled
            */

            BranchTracer::Pause(this);

            DbgPrint("[DebugFaultHandler]   Finished single stepping %p \n", guest_vmcb.save_state_area.rip);

            Instrumentation::InvokeHook(this, Instrumentation::sandbox_readwrite);
        }
        else
        {
            InjectException(EXCEPTION_VECTOR::Debug, FALSE, 0);
        }
    }
    else
    {
        InjectException(EXCEPTION_VECTOR::Debug, FALSE, 0);
    }
}