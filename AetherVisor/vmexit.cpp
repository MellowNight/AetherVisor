#include "vmexit.h"

void InjectException(VcpuData* core_data, int vector, int error_code)
{
    EventInjection event_injection;

    event_injection.vector = vector;
    event_injection.type = 3;
    event_injection.valid = 1;

    event_injection.push_error_code = 1;
    event_injection.error_code = error_code;

    core_data->guest_vmcb.control_area.EventInj = event_injection.fields;
}

void HandleMsrExit(VcpuData* VpData, GuestRegisters* GuestRegisters)
{
    auto msr_id = GuestRegisters->rcx;

    LARGE_INTEGER msr_value;

    msr_value.QuadPart = __readmsr(msr_id);

    switch (msr_id)
    {
    case MSR::efer:
    {
        auto efer = (MsrEfer*)&msr_value.QuadPart;
        Logger::Get()->Log(" MSR::efer caught, msr_value.QuadPart = %p \n", msr_value.QuadPart);

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
void HandleVmmcall(VcpuData* VpData, GuestRegisters* GuestRegisters, bool* EndVM)
{
    auto id = GuestRegisters->rcx;

    switch (id)
    {
        case VMMCALL_ID::set_npt_hook:
        {
            NptHooks::SetNptHook(
                VpData,
                (void*)GuestRegisters->rdx,
                (uint8_t*)GuestRegisters->r8,
                0,0
            );

            break;
        }
        case VMMCALL_ID::disable_hv:
        {    
            DbgPrint("[AMD-Hypervisor] - disable_hv vmmcall id %p \n", id);

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

extern "C" bool HandleVmexit(VcpuData* core_data, GuestRegisters* GuestRegisters)
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
            core_data->NestedPageFaultHandler(GuestRegisters);
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
        default:
        {
            DbgPrint("[VMEXIT] vmexit with unknown reason %p ! guest VMCB at %p \n",
                core_data->guest_vmcb.control_area.ExitCode, &core_data->guest_vmcb);

            DbgPrint("[VMEXIT] RIP is %p \n", core_data->guest_vmcb.save_state_area.Rip);

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
        
        __writecr3(core_data->guest_vmcb.save_state_area.Cr3);
        Logger::Get()->Log("[VMEXIT] NRip is %p \n", core_data->guest_vmcb.control_area.NRip);

        __svm_stgi();
        _disable();

        MsrEfer msr;

        msr.flags = __readmsr(MSR::efer);
        msr.svme = 0;

        __writemsr(MSR::efer, msr.flags);
        __writeeflags(core_data->guest_vmcb.save_state_area.Rflags.Flags);

        GuestRegisters->rcx = core_data->guest_vmcb.save_state_area.Rsp;
        GuestRegisters->rbx = core_data->guest_vmcb.control_area.NRip;
        __svm_vmload(core_data->guest_vmcb_physicaladdr);

        Logger::Get()->Log("ending hypervisor... \n");
    }

    return end_hypervisor;
}