#include "debug_exception.h"
#include "npt_sandbox.h"

void HandleDebugException(VcpuData* vcpu_data)
{
    DR6 dr6;

    dr6.Flags = vcpu_data->guest_vmcb.save_state_area.Dr6;

    if (dr6.SingleInstruction == 1) 
    {
        DbgPrint("HandleDebugException2222() \n");

        RFLAGS rflag{ rflag.Flags = vcpu_data->guest_vmcb.save_state_area.Rflags, rflag.TrapFlag = 0 };

        /*	single-step the read/write in the ncr3 that allows all pages to be executable	*/

        vcpu_data->guest_vmcb.save_state_area.Rflags = rflag.Flags;

        if (vcpu_data->guest_vmcb.control_area.NCr3 == Hypervisor::Get()->ncr3_dirs[sandbox_single_step])
        {
            DbgPrint("Finished single stepping %p \n", vcpu_data->guest_vmcb.save_state_area.Rip);

            vcpu_data->guest_vmcb.control_area.NCr3 = Hypervisor::Get()->ncr3_dirs[sandbox];

            vcpu_data->guest_vmcb.save_state_area.Rsp -= 8;

            *(uintptr_t*)vcpu_data->guest_vmcb.save_state_area.Rsp = vcpu_data->guest_vmcb.save_state_area.Rip;

            vcpu_data->guest_vmcb.save_state_area.Rip = (uintptr_t)Sandbox::sandbox_hooks[Sandbox::readwrite_handler];

            vcpu_data->guest_vmcb.control_area.VmcbClean &= 0xFFFFFFEF;
            vcpu_data->guest_vmcb.control_area.TlbControl = 1;
        }
    }
    else      
    {
        InjectException(vcpu_data, EXCEPTION_VECTOR::Debug, FALSE, 0);
    }
}
