#include "vmexit.h"
#include "npt_sandbox.h"
#include "branch_tracer.h"

void HandleBreakpoint(VcpuData* vcpu_data, GuestRegisters* guest_ctx)
{
    auto vmroot_cr3 = __readcr3();

    __writecr3(vcpu_data->guest_vmcb.save_state_area.Cr3.Flags);

    auto guest_rip = vcpu_data->guest_vmcb.save_state_area.Rip;

    if (BranchTracer::initialized && guest_rip == BranchTracer::start_address && !BranchTracer::thread_id)
    {
        DbgPrint("vcpu_data->guest_vmcb.save_state_area.Rip = %p \n", guest_rip);

        NPTHooks::ForEachHook(
            [](auto hook_entry, auto data)-> auto {

                if (hook_entry->address == data)
                {
                    DbgPrint("hook_entry->address == data, found stealth breakpoint! \n");

                    UnsetHook(hook_entry);
                }

                return false;
            },
            (void*)guest_rip
        );

        /*  capture the ID of the target thread */

        BranchTracer::Start(vcpu_data);

        BranchTracer::thread_id = PsGetCurrentThreadId();

        int processor_id = KeGetCurrentProcessorNumber();

        KAFFINITY affinity = Utils::Exponent(2, processor_id);

        KeSetSystemAffinityThread(affinity);

        vcpu_data->guest_vmcb.control_area.VmcbClean &= 0xFFFFFFEF;
        vcpu_data->guest_vmcb.control_area.TlbControl = 1;
    }
    else
    {
        InjectException(vcpu_data, EXCEPTION_VECTOR::Breakpoint, FALSE, 0);
    }


    __writecr3(vmroot_cr3);
}