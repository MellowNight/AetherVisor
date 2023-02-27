#include "vmexit.h"
#include "npt_sandbox.h"
#include "branch_tracer.h"

void VcpuData::BreakpointHandler(GuestRegisters* guest_ctx)
{
  //  DbgPrint("[BreakpointHandler]   BranchTracer::initialized %i, BranchTracer::start_address %p, BranchTracer::thread_id %i ! \n", BranchTracer::initialized, BranchTracer::start_address, BranchTracer::thread_id);

    auto guest_rip = guest_vmcb.save_state_area.rip;
 //   DbgPrint("[BreakpointHandler]   guest_rip %p \n\n", guest_rip);

    if (BranchTracer::initialized && guest_rip == BranchTracer::start_address)
    {
        /*  capture the ID of the target thread & start the tracer  */

        BranchTracer::Start(this);
    }
    else
    {
        InjectException(EXCEPTION_VECTOR::Breakpoint, FALSE, 0);
    }
}