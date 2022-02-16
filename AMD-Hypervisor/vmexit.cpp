#include "itlb_hook.h"
#include "npt_hook.h"
#include "hv_interface.h"
#include "logging.h"
#include "prepare_vm.h"

enum VMEXIT
{
    CPUID = 0x72,
    MSR = 0x7C,
    VMRUN = 0x80,
    VMMCALL = 0x81,
    NPF = 0x400,
    PF = 0x4E,
    INVALID = -1,
    GP = 0x4D,
    DB = 0x41,
};

void InjectException(CoreVmcbData* core_data, int vector, int error_code = 0)
{
    EventInjection event_injection;

    event_injection.vector = vector;
    event_injection.type = 3;
    event_injection.valid = 1;

    if (error_code != 0)
    {
        event_injection.push_error_code = 1;
        event_injection.error_code = error_code;
    }

    core_data->guest_vmcb.control_area.EventInj = event_injection.fields;
}

void HandleCpuidExit(CoreVmcbData* VpData, GPRegs* GuestRegisters)
{
    VpData->guest_vmcb.save_state_area.Rip = VpData->guest_vmcb.control_area.NRip;
}

void HandleMsrExit(CoreVmcbData* VpData, GPRegs* GuestRegisters)
{
    VpData->guest_vmcb.save_state_area.Rip = VpData->guest_vmcb.control_area.NRip;
}


void HandleVmmcall(CoreVmcbData* VpData, GPRegs* GuestRegisters, bool* EndVM)
{
    VpData->guest_vmcb.save_state_area.Rip = VpData->guest_vmcb.control_area.NRip;
}


extern "C" bool HandleVmexit(CoreVmcbData* VpData, GPRegs* GuestRegisters)
{
    /*	load host extra state	*/
    __svm_vmload(VpData->host_vmcb_physicaladdr);

    bool EndVm = false;

    switch ((int)VpData->guest_vmcb.control_area.ExitCode) {

    case VMEXIT::CPUID:
    {
        HandleCpuidExit(VpData, GuestRegisters);
        break;
    }
    case VMEXIT::MSR: 
    {
        HandleMsrExit(VpData, GuestRegisters);
        break;
    }
    case VMEXIT::VMRUN: 
    {
        InjectException(VpData, 13);
        break;
    }
    case VMEXIT::VMMCALL: 
    {
        HandleVmmcall(VpData, GuestRegisters, &EndVm);
        break;
    }
    case VMEXIT::NPF:
    {
        HandleNestedPageFault(VpData, GuestRegisters);
        break;
    }
    case VMEXIT::PF:
    {
        TlbHooker::HandleTlbHook(VpData, GuestRegisters);
        break;
    }
    case VMEXIT::GP: 
    {
        InjectException(VpData, 13, 0xC0000005);
        break;
    }
    case VMEXIT::INVALID: 
    {
        SegmentAttribute CsAttrib;
        CsAttrib.as_uint16 = VpData->guest_vmcb.save_state_area.CsAttrib;

        IsProcessorReadyForVmrun(&VpData->guest_vmcb, CsAttrib);

        break;
    }
    default:
        Logger::Log(L"[VMEXIT] huh?? wtf why did I exit ?? exit code %p \n",
            VpData->guest_vmcb.control_area.ExitCode);
        break;
    }

    if (EndVm) 
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

        __svm_vmload(VpData->guest_vmcb_physicaladdr);

        __svm_stgi();
        _disable();

        MsrEfer msr;

        msr.flags = __readmsr(MSR::EFER);
        msr.svme = 0;

        __writemsr(MSR::EFER, msr.flags);
        __writeeflags(VpData->guest_vmcb.save_state_area.Rflags);

        GuestRegisters->rcx = VpData->guest_vmcb.save_state_area.Rsp;
        GuestRegisters->rbx = VpData->guest_vmcb.control_area.NRip;
    }

    return EndVm;
}