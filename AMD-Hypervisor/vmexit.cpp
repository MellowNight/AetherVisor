#include "vmexit.h"

void InjectException(CoreVmcbData* core_data, int vector, int error_code)
{
    EventInjection event_injection;

    event_injection.vector = vector;
    event_injection.type = 3;
    event_injection.valid = 1;

    event_injection.push_error_code = 1;
    event_injection.error_code = error_code;

    core_data->guest_vmcb.control_area.EventInj = event_injection.fields;
}

void HandleMsrExit(CoreVmcbData* VpData, GPRegs* GuestRegisters)
{
    auto msr_id = GuestRegisters->rcx;

    LARGE_INTEGER msr_value;

    msr_value.QuadPart = __readmsr(msr_id);

    switch (msr_id)
    {
    case MSR::EFER:
    {
        auto efer = (MsrEfer*)&msr_value.QuadPart;
        Logger::Log(" MSR::EFER caught, msr_value.QuadPart = %p \n", msr_value.QuadPart);

        efer->svme = 0;
        break;
    }
    default:
        break;
    }

    VpData->guest_vmcb.save_state_area.Rax = msr_value.LowPart;
    GuestRegisters->rdx = msr_value.HighPart;

    VpData->guest_vmcb.save_state_area.Rip = VpData->guest_vmcb.control_area.NRip;
}

/*  HandleVmmcall only handles the vmmcall for 1 core. 
    It is the guest's responsibility to set thread affinity.
*/
void HandleVmmcall(CoreVmcbData* VpData, GPRegs* GuestRegisters, bool* EndVM)
{
    auto id = GuestRegisters->rcx;

    switch (id)
    {
        case VMMCALL_ID::set_tlb_hook:
        {
            TlbHooks::SetTlbHook(
                (void*)GuestRegisters->rdx,
                (uint8_t*)GuestRegisters->r8,
                GuestRegisters->r9
            );
            
            break;
        }
        case VMMCALL_ID::set_npt_hook:
        {
            NptHooks::SetNptHook(
                VpData,
                (void*)GuestRegisters->rdx,
                (uint8_t*)GuestRegisters->r8,
                GuestRegisters->r9
            );

            break;
        }
        case VMMCALL_ID::disable_hv:
        {    
            Logger::Log("[AMD-Hypervisor] - disable_hv vmmcall id %p \n", id);

            *EndVM = true;
            break;
        }
        default: 
        {
            break;
        }
    }

    VpData->guest_vmcb.save_state_area.Rip = VpData->guest_vmcb.control_area.NRip;
}

extern "C" bool HandleVmexit(CoreVmcbData* core_data, GPRegs* GuestRegisters)
{
    /*	load host extra state	*/
    __svm_vmload(core_data->host_vmcb_physicaladdr);

    bool end_hypervisor = false;		

    switch (core_data->guest_vmcb.control_area.ExitCode) 
    {
        case VMEXIT::MSR: 
        {
            HandleMsrExit(core_data, GuestRegisters);
            break;
        }
        case VMEXIT::VMRUN: 
        {
            InjectException(core_data, 13);
            break;
        }
        case VMEXIT::VMMCALL: 
        {
            HandleVmmcall(core_data, GuestRegisters, &end_hypervisor);
            break;
        }
        case VMEXIT::NPF:
        {
            HandleNestedPageFault(core_data, GuestRegisters);
            break;
        }
        case VMEXIT::PF:
        {
            TlbHooks::HandlePageFaultTlb(core_data, GuestRegisters);
            //MpkHooks::HandlePageFaultMpk(core_data, GuestRegisters);

            break;
        }
        case VMEXIT::GP: 
        {
            InjectException(core_data, 13, 0xC0000005);
            break;
        }
        case VMEXIT::INVALID: 
        {
            SegmentAttribute CsAttrib;
            CsAttrib.as_uint16 = core_data->guest_vmcb.save_state_area.CsAttrib;

            IsProcessorReadyForVmrun(&core_data->guest_vmcb, CsAttrib);

            break;
        }
        case VMEXIT::BP:
        {
            TlbHooks::HandleTlbHookBreakpoint(core_data);
            break;
        }
        default:
        {
            Logger::Log("[VMEXIT] vmexit with unknown reason %p ! guest vmcb at %p \n",
                core_data->guest_vmcb.control_area.ExitCode, &core_data->guest_vmcb);

            Logger::Log("[VMEXIT] RIP is %p \n", core_data->guest_vmcb.save_state_area.Rip);

            __debugbreak();
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

        __svm_vmload(core_data->guest_vmcb_physicaladdr);
        __writecr3(core_data->guest_vmcb.save_state_area.Cr3);

        __svm_stgi();
        _disable();

        MsrEfer msr;

        msr.flags = __readmsr(MSR::EFER);
        msr.svme = 0;

        __writemsr(MSR::EFER, msr.flags);
        __writeeflags(core_data->guest_vmcb.save_state_area.Rflags);

        GuestRegisters->rcx = core_data->guest_vmcb.save_state_area.Rsp;
        GuestRegisters->rbx = core_data->guest_vmcb.control_area.NRip;

        Logger::Log("ending hypervisor... \n");
    }

    return end_hypervisor;
}