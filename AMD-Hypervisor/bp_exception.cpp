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

        /*  clean TLB after removing the NPT hook   */

        vcpu_data->guest_vmcb.control_area.VmcbClean &= 0xFFFFFFEF;
        vcpu_data->guest_vmcb.control_area.TlbControl = 1;

        /*  capture the ID of the target thread & start the tracer  */

        BranchTracer::Start(vcpu_data);
    }
    else
    {
        InjectException(vcpu_data, EXCEPTION_VECTOR::Breakpoint, FALSE, 0);
    }


    __writecr3(vmroot_cr3);
}