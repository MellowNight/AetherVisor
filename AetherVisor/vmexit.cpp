#include "vmexit.h"
#include "npt_sandbox.h"
#include "vmexit.h"
#include "disassembly.h"

void VcpuData::InjectException(int vector, bool push_error, int error_code)
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

    guest_vmcb.control_area.event_inject = event_inject.fields;
}

extern "C" bool HandleVmexit(VcpuData * vcpu, GuestRegisters * guest_ctx, PhysMemAccess * physical_mem)
{
    /*	load host extra state	*/

    __svm_vmload(vcpu->host_vmcb_physicaladdr);

    bool end_hypervisor = false;

    switch (vcpu->guest_vmcb.control_area.exit_code)
    {
    case VMEXIT::MSR:
    {
        vcpu->MsrExitHandler(guest_ctx);

        break;
    }
    case VMEXIT::DB:
    {
        vcpu->DebugFaultHandler(guest_ctx);

        break;
    }
    case VMEXIT::VMRUN:
    {
        vcpu->InjectException(EXCEPTION_VECTOR::GeneralProtection, false, 0);

        break;
    }
    case VMEXIT::VMMCALL:
    {
        vcpu->VmmcallHandler(guest_ctx, &end_hypervisor);

        break;
    }
    case VMEXIT::BP:
    {
        vcpu->BreakpointHandler(guest_ctx);

        break;
    }
    case VMEXIT::NPF:
    {
        vcpu->NestedPageFaultHandler(guest_ctx);

        break;
    }
    case VMEXIT::UD:
    {
        vcpu->InvalidOpcodeHandler(guest_ctx, physical_mem);

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

        EferMsr msr;

        msr.flags = __readmsr(MSR::efer);
        msr.svme = 0;

        // 5. disable SVM
        __writemsr(MSR::efer, msr.flags);

        // 6. load the guest value of EFLAGS
        __writeeflags(vcpu->guest_vmcb.save_state_area.rflags.Flags);

        // 7. restore these values later
        guest_ctx->rcx = vcpu->guest_vmcb.save_state_area.rsp;
        guest_ctx->rbx = vcpu->guest_vmcb.control_area.nrip;

        Logger::Get()->Log("ending hypervisor... \n");
    }

    return end_hypervisor;
}