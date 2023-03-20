#include "vmexit.h"
#include "npt_sandbox.h"
#include "vmexit.h"
#include "disassembly.h"

void VcpuData::InjectException(int vector, bool push_error, int error_code)
{
    EventInjection event_inject = { 0 };

    event_inject.vector = vector;
    event_inject.type = 3;
    event_inject.valid = 1;

    if (push_error)
    {
        event_inject.push_error_code = 1;
        event_inject.error_code = error_code;
    }

    guest_vmcb.control_area.event_inject = event_inject.fields;
}

extern "C" bool HandleVmexit(VcpuData * vcpu, GuestRegisters * guest_ctx)
{
    /*	load host extra state	*/

    __svm_vmload(vcpu->host_vmcb_physicaladdr);

    bool end_hypervisor = false;

    vcpu->suppress_nrip_increment = FALSE;

    switch (vcpu->guest_vmcb.control_area.exit_code)
    {
    case VMEXIT::MSR:
    {
        vcpu->MsrExitHandler(guest_ctx);

        break;
    }
    case VMEXIT::DB:
    {        
        auto vmroot_cr3 = __readcr3();
        
        __writecr3(vcpu->guest_vmcb.save_state_area.cr3.Flags);

        vcpu->DebugFaultHandler(guest_ctx);

        __writecr3(vmroot_cr3);

        break;
    }
    case VMEXIT::VMRUN:
    {
        vcpu->InjectException(EXCEPTION_VECTOR::InvalidOpcode, false, 0);

        break;
    }
    case VMEXIT::VMMCALL:
    {		
        auto vmroot_cr3 = __readcr3();
        
        __writecr3(vcpu->guest_vmcb.save_state_area.cr3.Flags);

        vcpu->VmmcallHandler(guest_ctx, &end_hypervisor);

        __writecr3(vmroot_cr3);

        break;
    }
    case VMEXIT::BP:
    {    
        auto vmroot_cr3 = __readcr3();

        __writecr3(vcpu->guest_vmcb.save_state_area.cr3.Flags);

        vcpu->BreakpointHandler(guest_ctx);

        __writecr3(vmroot_cr3);

        break;
    }
    case VMEXIT::NPF:
    {        
        auto vmroot_cr3 = __readcr3();

        __writecr3(vcpu->guest_vmcb.save_state_area.cr3.Flags);

        vcpu->NestedPageFaultHandler(guest_ctx);

        __writecr3(vmroot_cr3);

        break;
    }
    case VMEXIT::UD:
    {        
        auto vmroot_cr3 = __readcr3();

        __writecr3(vcpu->guest_vmcb.save_state_area.cr3.Flags);

        vcpu->InvalidOpcodeHandler(guest_ctx);

        __writecr3(vmroot_cr3);

        break;
    }
    case VMEXIT::DR0_READ:
    case VMEXIT::DR6_READ:
    case VMEXIT::DR7_READ:
    {
        vcpu->DebugRegisterExit(guest_ctx);

        if (vcpu->suppress_nrip_increment == FALSE)
        {
            vcpu->guest_vmcb.save_state_area.rip = vcpu->guest_vmcb.control_area.nrip;
        }

        break;
    }
    case VMEXIT::PUSHF:
    {
        auto vmroot_cr3 = __readcr3();

        __writecr3(vcpu->guest_vmcb.save_state_area.cr3.Flags);

        vcpu->PushfExit(guest_ctx);

        __writecr3(vmroot_cr3);

        if (vcpu->suppress_nrip_increment == FALSE)
        {
            vcpu->guest_vmcb.save_state_area.rip = vcpu->guest_vmcb.control_area.nrip;
        }

        break;
    }
    case VMEXIT::POPF:
    {
        auto vmroot_cr3 = __readcr3();

        __writecr3(vcpu->guest_vmcb.save_state_area.cr3.Flags);

        vcpu->PopfExit(guest_ctx);

        __writecr3(vmroot_cr3);

        if (vcpu->suppress_nrip_increment == FALSE)
        {
            vcpu->guest_vmcb.save_state_area.rip = vcpu->guest_vmcb.control_area.nrip;
        }

        break;
    }
    case VMEXIT::INVALID:
    {
        DbgPrint("VMEXIT::INVALID!! ! \n");
       // auto cs_attrib = vcpu->guest_vmcb.save_state_area.cs_attrib;
        // IsCoreReadyForVmrun(&vcpu->guest_vmcb, cs_attrib);

        vcpu->guest_vmcb.save_state_area.rip = vcpu->guest_vmcb.control_area.nrip;

        break;
    }
    default:
    {        
        DbgPrint("unknown VMEXIT 0x%p \n", vcpu->guest_vmcb.control_area.exit_code);

        KeBugCheckEx(MANUALLY_INITIATED_CRASH, vcpu->guest_vmcb.control_area.exit_code, 
            vcpu->guest_vmcb.control_area.exit_info1, vcpu->guest_vmcb.control_area.exit_info2, vcpu->guest_vmcb.save_state_area.rip);

        break;
    }
    }

    if (end_hypervisor)
    {
        // 1. Load guest CR3 context
        __writecr3(vcpu->guest_vmcb.save_state_area.cr3.Flags);

        // 2. Load guest hidden context
        __svm_vmload(vcpu->guest_vmcb_physicaladdr);

        // 3. Enable global interrupt flag
        __svm_stgi();

        // 4. Disable interrupt flag in EFLAGS (to safely disable SVM)
        _disable();

        EFER_MSR msr;

        msr.value = __readmsr(MSR::efer);
        msr.svme = 0;

        // 5. disable SVM
        __writemsr(MSR::efer, msr.value);

        // 6. load the guest value of EFLAGS
        __writeeflags(vcpu->guest_vmcb.save_state_area.rflags.Flags);

        // 7. restore these values later
        guest_ctx->rcx = vcpu->guest_vmcb.save_state_area.rsp;
        guest_ctx->rbx = vcpu->guest_vmcb.control_area.nrip;

        Logger::Get()->Log("ending hypervisor... \n");
    }

    return end_hypervisor;
}