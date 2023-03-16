#include "vmexit.h"
#include "npt_sandbox.h"
#include "branch_tracer.h"
#include "instrumentation_hook.h"

using namespace Instrumentation;

void VcpuData::DebugFaultHandler(GuestRegisters* guest_ctx)
{
    if (guest_vmcb.save_state_area.rip == BranchTracer::resume_address)
    {
        /*	transition out of branch callback, continue branch single-stepping	*/

       // DbgPrint("[UpdateState]		Branch hook finished, guest_rip %p \n", guest_vmcb.save_state_area.rip);

        BranchTracer::resume_address = NULL;

        auto tls_params = Utils::GetTlsPtr<BranchTracer::TlsParams>(guest_vmcb.save_state_area.gs_base, callbacks[branch].tls_params_idx);

        (*tls_params)->callback_pending = false;
        (*tls_params)->resume_address = guest_vmcb.save_state_area.rip;

        /*  capture the ID of the target thread & start the tracer  */

        BranchTracer::Resume(this);

        guest_vmcb.save_state_area.dr7.GlobalBreakpoint0 = 0;
        guest_vmcb.save_state_area.dr6.SingleInstruction = 0;

        return;
    }

    DR6 dr6 = guest_vmcb.save_state_area.dr6;

    if (dr6.SingleInstruction == 1)
    {
       // DbgPrint("[DebugFaultHandler]   guest_rip %p \n", guest_vmcb.save_state_area.rip);

        if (BranchTracer::active == true)
        {
            BranchTracer::UpdateState(this, guest_ctx);
        }
        else if (guest_vmcb.control_area.ncr3 == Hypervisor::Get()->ncr3_dirs[sandbox_single_step])
        {
            /*  Transfer RIP to the instrumentation hook for read instructions.
                Single-stepping mode => completely disabled
            */

            BranchTracer::Pause(this);

            guest_vmcb.save_state_area.rflags.TrapFlag = 0;
            guest_vmcb.save_state_area.dr6.SingleInstruction = 0;

            // DbgPrint("[DebugFaultHandler]   Finished single stepping %p \n", guest_vmcb.save_state_area.rip);

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