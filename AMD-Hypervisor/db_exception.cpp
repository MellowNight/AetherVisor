#include "db_exception.h"
#include "npt_sandbox.h"
#include "branch_tracer.h"

void HandleDebugException(VcpuData* vcpu_data, GeneralRegisters* guest_ctx)
{
    auto vmroot_cr3 = __readcr3();

    __writecr3(vcpu_data->guest_vmcb.save_state_area.Cr3);

    auto guest_rip = vcpu_data->guest_vmcb.save_state_area.Rip;

    DR6 dr6;
    dr6.Flags = vcpu_data->guest_vmcb.save_state_area.Dr6;

    if (BranchTracer::initialized && guest_rip == BranchTracer::start_address)
    {
        /*  capture the ID of the target thread */

        DbgPrint("vcpu_data->guest_vmcb.save_state_area.gsbase  = %p \n", vcpu_data->guest_vmcb.save_state_area.GsBase);
      
        auto pcrb = (PETHREAD*)((_KPCR*)vcpu_data->guest_vmcb.save_state_area.GsBase)->CurrentPrcb;
        
        DbgPrint("((_KPCR*)vcpu_data->guest_vmcb.save_state_area.GsBase)->CurrentPrcb = %p \n",
            pcrb);       

        auto thread_id = PsGetThreadId(*(pcrb + 1));
        
        DbgPrint("current ETHREAD ID = %p \n", thread_id);

        BranchTracer::thread_id = thread_id;

        return;
    }

    if (dr6.SingleInstruction == 1) 
    {
        vcpu_data->guest_vmcb.save_state_area.Rflags &= (~((uint64_t)1 << RFLAGS_TRAP_FLAG_BIT));

        /*	single-step the sandboxed read/write in the ncr3 that allows all pages to be executable	*/

        if (vcpu_data->guest_vmcb.control_area.NCr3 == Hypervisor::Get()->ncr3_dirs[sandbox_single_step])
        {
            DbgPrint("Finished single stepping %p \n", vcpu_data->guest_vmcb.save_state_area.Rip);

            Sandbox::InstructionInstrumentation(vcpu_data, guest_rip, guest_ctx, Sandbox::readwrite_handler, true);
        }
    }
    else      
    {
        InjectException(vcpu_data, EXCEPTION_VECTOR::Debug, FALSE, 0);
    }

    __writecr3(vmroot_cr3);
}
