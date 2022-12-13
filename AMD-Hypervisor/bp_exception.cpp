#include "vmexit.h"
#include "npt_sandbox.h"
#include "branch_tracer.h"

void HandleBreakpoint(VcpuData* vcpu_data, GuestRegisters* guest_ctx)
{
    auto guest_rip = vcpu_data->guest_vmcb.save_state_area.Rip;

    DbgPrint("vcpu_data->guest_vmcb.save_state_area.Rip = %p \n", guest_rip);

    if (BranchTracer::initialized && guest_rip == BranchTracer::start_address&& !BranchTracer::thread_id)
    {
        vcpu_data->guest_vmcb.save_state_area.Rip -= 1;

        NPTHooks::UnsetHook(BranchTracer::capture_thread_bp);

        /*  capture the ID of the target thread */

        auto pcrb = (PETHREAD*)((_KPCR*)__readmsr(0xC0000101))->CurrentPrcb;

        DbgPrint("((_KPCR*)GsBase)->CurrentPrcb = %p \n", pcrb);

        auto thread_id = PsGetThreadId(*(pcrb + 1));

        BranchTracer::thread_id = thread_id;

        DbgPrint(" BranchTracer::thread_id = %p PsGetCurrentThreadId = %p \n", BranchTracer::thread_id, PsGetCurrentThreadId());

        int processor_id = KeGetCurrentProcessorNumber();

        KAFFINITY affinity = Utils::Exponent(2, processor_id);

        KeSetSystemAffinityThread(affinity);

        BranchTracer::Start(vcpu_data);
    }
    else
    {
        InjectException(vcpu_data, EXCEPTION_VECTOR::Breakpoint, FALSE, 0);
    }
}