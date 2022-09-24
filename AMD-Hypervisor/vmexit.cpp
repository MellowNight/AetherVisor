#include "vmexit.h"

void InjectException(CoreData* core_data, int vector, bool push_error_code, int error_code)
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

void HandleMsrExit(CoreData* core_data, GPRegs* guest_regs)
{
    uint32_t msr_id = guest_regs->rcx & (uint32_t)0xFFFFFFFF;

    if (msr_id != 0x000C001029A)
    {
    //    DbgPrint("msr_id 0x%p guest_regs->rcx 0x%p \n", msr_id, guest_regs->rcx);
    }

    if (!(((msr_id > 0) && (msr_id < 0x00001FFF)) || ((msr_id > 0xC0000000) && (msr_id < 0xC0001FFF)) || (msr_id > 0xC0010000) && (msr_id < 0xC0011FFF)))
    {
        InjectException(core_data, EXCEPTION_GP_FAULT, true, 0);
        core_data->guest_vmcb.save_state_area.Rip = core_data->guest_vmcb.control_area.NRip;

        return;
    }

    LARGE_INTEGER msr_value;

    msr_value.QuadPart = __readmsr(msr_id);

    switch (msr_id)
    {
    case MSR::EFER:
    {
        auto efer = (MsrEfer*)&msr_value.QuadPart;
        Logger::Get()->Log(" MSR::EFER caught, msr_value.QuadPart = %p \n", msr_value.QuadPart);

        efer->svme = 0;
        break;
    }
    default:
        break;
    }

    core_data->guest_vmcb.save_state_area.Rax = msr_value.LowPart;
    guest_regs->rdx = msr_value.HighPart;

    core_data->guest_vmcb.save_state_area.Rip = core_data->guest_vmcb.control_area.NRip;
}

/*  HandleVmmcall only handles the vmmcall for 1 core. 
    It is the guest's responsibility to set thread affinity.
*/
void HandleVmmcall(CoreData* vmcb_data, GPRegs* GuestRegisters, bool* EndVM)
{
    auto id = GuestRegisters->rcx;

    switch (id)
    {
        case VMMCALL_ID::remove_npt_hook:
        {
            auto vmroot_cr3 = __readcr3();
            __writecr3(vmcb_data->guest_vmcb.save_state_area.Cr3);
           
            NptHooks::ForEachHook(
                [](NptHooks::NptHook* hook_entry, void* data)->bool {

                    if (hook_entry->tag == (uintptr_t)data)
                    {
                        NptHooks::UnsetHook(hook_entry);
                    }
                    return true;
                },
               (void*)GuestRegisters->rdx
            );

            __writecr3(vmroot_cr3);

            break;
        }
        case VMMCALL_ID::set_npt_hook:
        {
            LARGE_INTEGER length_tag;
            length_tag.QuadPart = GuestRegisters->r9;

            NptHooks::SetNptHook(
                vmcb_data,
                (void*)GuestRegisters->rdx,
                (uint8_t*)GuestRegisters->r8,
                length_tag.HighPart,
                length_tag.LowPart
            );

            break;
        }
        case VMMCALL_ID::disable_hv:
        {
            Logger::Get()->Log("[AMD-Hypervisor] - disable_hv vmmcall id %p \n", id);

            *EndVM = true;
            break;
        }
        case VMMCALL_ID::is_hv_present:
        {
            break;
        }
        default: 
        {
            InjectException(vmcb_data, EXCEPTION_INVALID_OPCODE, false, 0);
            return;
        }
    }

    vmcb_data->guest_vmcb.save_state_area.Rip = vmcb_data->guest_vmcb.control_area.NRip;
}

extern "C" bool HandleVmexit(CoreData* core_data, GPRegs* GuestRegisters)
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
            InjectException(core_data, EXCEPTION_GP_FAULT, false, 0);
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
        case VMEXIT::INVALID: 
        {
            SegmentAttribute CsAttrib;
            CsAttrib.as_uint16 = core_data->guest_vmcb.save_state_area.CsAttrib;

            IsProcessorReadyForVmrun(&core_data->guest_vmcb, CsAttrib);

            break;
        }
        case VMEXIT::VMEXIT_MWAIT_CONDITIONAL:
        {
            core_data->guest_vmcb.save_state_area.Rip = core_data->guest_vmcb.control_area.NRip;
            break;
        }
        case 0x55: // CET shadow stack exception
        {
            InjectException(core_data, 0x55, TRUE, 0);
            break;
        }
        default:
        {
           /* KeBugCheckEx(MANUALLY_INITIATED_CRASH, core_data->guest_vmcb.control_area.ExitCode, core_data->guest_vmcb.control_area.ExitInfo1, core_data->guest_vmcb.control_area.ExitInfo2, core_data->guest_vmcb.save_state_area.Rip);

            Logger::Get()->Log("[VMEXIT] vmexit with unknown reason %p ! guest vmcb at %p \n",
                core_data->guest_vmcb.control_area.ExitCode, &core_data->guest_vmcb);

            Logger::Get()->Log("[VMEXIT] RIP is %p \n", core_data->guest_vmcb.save_state_area.Rip);

            __debugbreak();*/

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
        __svm_vmload(core_data->guest_vmcb_physicaladdr);

        __svm_stgi();
        _disable();

        MsrEfer msr;

        msr.flags = __readmsr(MSR::EFER);
        msr.svme = 0;

        __writemsr(MSR::EFER, msr.flags);
        __writeeflags(core_data->guest_vmcb.save_state_area.Rflags);

        GuestRegisters->rcx = core_data->guest_vmcb.save_state_area.Rsp;
        GuestRegisters->rbx = core_data->guest_vmcb.control_area.NRip;

        Logger::Get()->Log("ending hypervisor... \n");
    }

    return end_hypervisor;
}