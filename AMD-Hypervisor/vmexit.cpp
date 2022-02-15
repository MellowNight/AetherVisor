#include "npt_hook.h"
#include "npt_setup.h"
#include "hv_interface.h"
#include "logging.h"


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

uintptr_t GuestRip = 0;
void HandleNestedPageFault(CoreVmcbData* VpData, GeneralPurposeRegs* GuestContext)
{
    NestedPageFaultInfo1 exit_info1;

    exit_info1.as_uint64 = VpData->guest_vmcb.control_area.ExitInfo1;

    uintptr_t fail_address = VpData->guest_vmcb.control_area.ExitInfo2;

    PHYSICAL_ADDRESS NCr3;

    NCr3.QuadPart = VpData->guest_vmcb.control_area.NCr3;

    GuestRip = VpData->guest_vmcb.save_state_area.Rip;


    if (exit_info1.fields.valid == 0) 
    {

        int num_bytes = VpData->guest_vmcb.control_area.NumOfBytesFetched;

        auto insn_bytes = VpData->guest_vmcb.control_area.GuestInstructionBytes;

        auto pml4_base = (PML4E_64*)MmGetVirtualForPhysical(NCr3);

        auto pte = AssignNPTEntry((PML4E_64*)pml4_base, fail_address, true);

        return;
    }


    if (exit_info1.fields.execute == 1) 
    {
        NptHookEntry* nptHook = GetHookByPhysicalPage(global_hypervisor_data, fail_address);

        bool Switch = true;

        int length = 10;

        if (PAGE_ALIGN(GuestRip + length) != PAGE_ALIGN(GuestRip)) {

            PT_ENTRY_64* N_Pte = Utils::GetPte((void*)
                MmGetPhysicalAddress(PAGE_ALIGN(GuestRip)).QuadPart, NCr3.QuadPart);


            PT_ENTRY_64* N_Pte2 = Utils::GetPte((void*)
                MmGetPhysicalAddress(PAGE_ALIGN(GuestRip + length)).QuadPart, NCr3.QuadPart);


            N_Pte->ExecuteDisable = 0;
            N_Pte2->ExecuteDisable = 0;

            Switch = false;
        }   

        VpData->guest_vmcb.control_area.VmcbClean &= 0xFFFFFFEF;
        VpData->guest_vmcb.control_area.TlbControl = 3;

        /*  switch to hook CR3, with hooks mapped or switch to innocent CR3, without any hooks  */
        if (Switch)
        {
            if (nptHook) 
            {
                VpData->guest_vmcb.control_area.NCr3 = global_hypervisor_data->noexecute_ncr3;
            }
            else 
            {
                VpData->guest_vmcb.control_area.NCr3 = global_hypervisor_data->normal_ncr3;
            }
        }
    }
}

void HandleCpuidExit(CoreVmcbData* VpData, GeneralPurposeRegs* GuestRegisters)
{
    VpData->guest_vmcb.save_state_area.Rip = VpData->guest_vmcb.control_area.NRip;
}

void HandleMsrExit(CoreVmcbData* VpData, GeneralPurposeRegs* GuestRegisters)
{
    VpData->guest_vmcb.save_state_area.Rip = VpData->guest_vmcb.control_area.NRip;
}


void HandleVmmcall(CoreVmcbData* VpData, GeneralPurposeRegs* GuestRegisters,
    bool* EndVM)
{
    int Registers[4];

    UINT64 Origin = GuestRegisters->rdx;
    UINT64 Leaf = GuestRegisters->rcx;

    if(Origin == KERNEL_VMMCALL)
    { 
        switch (Leaf) {

        case VMMCALL : 
        {
            Registers[0] = 'epyH'; /*	"HyperCheatzz"	*/
            Registers[1] = 'ehCr';
            Registers[2] = 'zzta';

            VpData->guest_vmcb.save_state_area.Rax = Registers[0];
            GuestRegisters->Rbx = Registers[1];
            GuestRegisters->Rcx = Registers[2];
            GuestRegisters->Rdx = Registers[3];

            break;
        }
        case VMMCALL::DISABLE_HOOKS: 
        {
            VpData->guest_vmcb.control_area.NCr3 = global_hypervisor_data->tertiary_cr3;
            break;
        }
        case VMMCALL::END_HV: 
        {
            *EndVM = true;
            break;
        }

        default:
            InjectException(VpData, 6);
            break;
        }
    }
    else if (Origin == USER_VMMCALL)
    {
        global_hypervisor_data->HvCommand = Leaf;
    }
    VpData->guest_vmcb.SaveStateArea.Rip = VpData->guest_vmcb.control_area.NRip;
}


extern "C" bool HandleVmexit(CoreVmcbData* VpData, GeneralPurposeRegs* GuestRegisters)
{
    /*	load host extra state	*/
    __svm_vmload(VpData->host_vmcb_physicaladdr);

    bool EndVm = false;

    switch ((int)VpData->guest_vmcb.control_area.ExitCode) {

    case VMEXIT::CPUID: {
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
    case VMEXIT::GP: 
    {
        InjectException(VpData, 13, 0xC0000005);
        break;
    }
    case VMEXIT::INVALID: 
    {
        SegmentAttribute CsAttrib;
        CsAttrib.as_uint16 = VpData->guest_vmcb.SaveStateArea.CsAttrib;

        IsProcessorReadyForVmrun(&VpData->guest_vmcb, CsAttrib);

        break;
    }
    default:
        DbgPrint("[VMEXIT] huh?? wtf why did I exit ?? exit code %p \n",
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