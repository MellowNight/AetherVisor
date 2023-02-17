#include "vmexit.h"
#include "npt_sandbox.h"
#include "branch_tracer.h"
#include "instrumentation_hook.h"

using namespace Instrumentation;

void VcpuData::DebugFaultHandler(GuestRegisters* guest_ctx)
{

    DR6 dr6 = guest_vmcb.save_state_area.dr6;

    if (dr6.SingleInstruction == 1)
    {
        DbgPrint("[DebugFaultHandler]   guest_vmcb.save_state_area.dr7.Flags & ((uint64_t)1 << 9)) = %i \n", guest_vmcb.save_state_area.dr7.Flags & ((uint64_t)1 << 9));
        DbgPrint("[DebugFaultHandler]   BranchTracer::range_base %p \n", BranchTracer::range_base);
        DbgPrint("[DebugFaultHandler]   BranchTracer::range_base + BranchTracer::range_size %p \n\n\n", BranchTracer::range_size + BranchTracer::range_base);

        if (BranchTracer::active == true)
        {
            BranchTracer::UpdateState(this);
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