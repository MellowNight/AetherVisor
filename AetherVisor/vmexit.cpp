#include "vmexit.h"
#include "npt_sandbox.h"
#include "msr.h"
#include "disassembly.h"

void InjectException(VcpuData* core_data, int vector, bool push_error, int error_code)
{
    EventInject event_inject = { 0 };

    event_inject.vector = vector;
    event_inject.type = 3;
    event_inject.valid = 1;
    
    if (push_error)
    {
        event_inject.push_error = 1;
        event_inject.error_code = error_code;
    }

    core_data->guest_vmcb.control_area.event_inject = event_inject.fields;
}

extern "C" bool HandleVmexit(VcpuData* vcpu_data, GuestRegs* guest_ctx)
{
    /*	load host extra state	*/

    __svm_vmload(vcpu_data->host_vmcb_physicaladdr);

    bool end_hypervisor = false;		

    switch (vcpu_data->guest_vmcb.control_area.exit_code)
    {
        case VMEXIT::MSR: 
        {
            MsrExitHandler(vcpu_data, guest_ctx);
            break;
        }
        case VMEXIT::DB:
        {  
            DebugExceptionHandler(vcpu_data, guest_ctx);

            break;
        }
        case VMEXIT::VMRUN: 
        {
            InjectException(vcpu_data, EXCEPTION_GP_FAULT, false, 0);
            break;
        }
        case VMEXIT::VMMCALL: 
        {            
            VmmcallHandler(vcpu_data, guest_ctx, &end_hypervisor);
            break;
        }
        case VMEXIT::BP:
        {     
            BreakpointHandler(vcpu_data, guest_ctx);

            break;
        }
        case VMEXIT::NPF:
        {        
            NestedPageFaultHandler(vcpu_data, guest_ctx);

            break;
        }
        case VMEXIT::INVALID: 
        {
            SegmentAttribute cs_attrib;

            cs_attrib.as_uint16 = vcpu_data->guest_vmcb.save_state_area.cs_attrib;

            IsCoreReadyForVmrun(&vcpu_data->guest_vmcb, cs_attrib);

            break;
        }
        default:
        {            
            KeBugCheckEx(MANUALLY_INITIATED_CRASH, 
                vcpu_data->guest_vmcb.control_area.exit_code, vcpu_data->guest_vmcb.control_area.exit_info1, vcpu_data->guest_vmcb.control_area.exit_info2, vcpu_data->guest_vmcb.save_state_area.rip);

            break;
        }
    }

    if (end_hypervisor) 
    {
        // 1. Load guest CR3 context
        __writecr3(vcpu_data->guest_vmcb.save_state_area.cr3.Flags);

        // 2. Load guest hidden context
        __svm_vmload(vcpu_data->guest_vmcb_physicaladdr);

        // 3. Enable global interrupt flag
        __svm_stgi();

        // 4. Disable interrupt flag in EFLAGS (to safely disable SVM)
        _disable();

        EferMsr msr;

        msr.flags = __readmsr(MSR::efer);
        msr.svme = 0;

        // 5. disable SVM
        __writemsr(MSR::efer, msr.flags);

        // 6. load the guest value of EFLAGS
        __writeeflags(vcpu_data->guest_vmcb.save_state_area.rflags.Flags);

        // 7. restore these values later
        guest_ctx->rcx = vcpu_data->guest_vmcb.save_state_area.rsp;
        guest_ctx->rbx = vcpu_data->guest_vmcb.control_area.nrip;

        Logger::Get()->Log("ending hypervisor... \n");
    }

    return end_hypervisor;
}