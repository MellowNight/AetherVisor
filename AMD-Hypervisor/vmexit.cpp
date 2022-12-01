#include "vmexit.h"
#include "npt_sandbox.h"
#include "db_exception.h"
#include "msr.h"

void InjectException(VcpuData* core_data, int vector, bool push_error_code, int error_code)
{
    EventInjection event_injection = { 0 };

    event_injection.vector = vector;
    event_injection.type = 3;
    event_injection.valid = 1;
    
    if (push_error_code)
    {
        event_injection.push_error_code = 1;
        event_injection.error_code = error_code;
    }

    core_data->guest_vmcb.control_area.EventInj = event_injection.fields;
}

extern "C" bool HandleVmexit(VcpuData* vcpu_data, GeneralRegisters* GuestRegisters)
{
    /*	load host extra state	*/

    __svm_vmload(vcpu_data->host_vmcb_physicaladdr);

    bool end_hypervisor = false;		

    switch (vcpu_data->guest_vmcb.control_area.ExitCode)
    {
        case VMEXIT::MSR: 
        {
            HandleMsrExit(vcpu_data, GuestRegisters);
            break;
        }
        case VMEXIT::DB:
        {
            HandleDebugException(vcpu_data);
            break;
        }
        case VMEXIT::VMRUN: 
        {
            InjectException(vcpu_data, EXCEPTION_GP_FAULT, false, 0);
            break;
        }
        case VMEXIT::VMMCALL: 
        {            
            HandleVmmcall(vcpu_data, GuestRegisters, &end_hypervisor);
            break;
        }
        case VMEXIT::NPF:
        {
            HandleNestedPageFault(vcpu_data, GuestRegisters);
            break;
        }
        case VMEXIT::INVALID: 
        {
            SegmentAttribute CsAttrib;

            CsAttrib.as_uint16 = vcpu_data->guest_vmcb.save_state_area.CsAttrib;

            IsProcessorReadyForVmrun(&vcpu_data->guest_vmcb, CsAttrib);

            break;
        }
        case VMEXIT::VMEXIT_MWAIT_CONDITIONAL:
        {
            vcpu_data->guest_vmcb.save_state_area.Rip = vcpu_data->guest_vmcb.control_area.NRip;
            break;
        }
        case 0x55: // CET shadow stack exception
        {
            InjectException(vcpu_data, 0x55, TRUE, 0);
            break;
        }
        default:
        {
           /* KeBugCheckEx(MANUALLY_INITIATED_CRASH, core_data->guest_vmcb.control_area.ExitCode, core_data->guest_vmcb.control_area.ExitInfo1, core_data->guest_vmcb.control_area.ExitInfo2, core_data->guest_vmcb.save_state_area.Rip);
           */

            break;
        }
    }

    if (end_hypervisor) 
    {
        /*
            When we end the VM operation, we just disable virtualization
            and jump to the guest context

            1. load guest state
            2. disable IF
            3. enable GIF
            4. disable SVME
            5. restore EFLAGS and re enable IF
            6. set RBX to RIP
            7. set RCX to RSP
            8. return and jump back
        */
        
        __writecr3(vcpu_data->guest_vmcb.save_state_area.Cr3);
        __svm_vmload(vcpu_data->guest_vmcb_physicaladdr);

        __svm_stgi();
        _disable();

        MsrEfer msr;

        msr.flags = __readmsr(MSR::EFER);
        msr.svme = 0;

        __writemsr(MSR::EFER, msr.flags);
        __writeeflags(vcpu_data->guest_vmcb.save_state_area.Rflags);

        GuestRegisters->rcx = vcpu_data->guest_vmcb.save_state_area.Rsp;
        GuestRegisters->rbx = vcpu_data->guest_vmcb.control_area.NRip;

        Logger::Get()->Log("ending hypervisor... \n");
    }

    return end_hypervisor;
}