#include "hook_handler.h"
#include "npt_hook.h"
#include "npt_setup.h"
#include "Command.h"
#include "Logging.h"
#include "Experiment.h"

extern bool
IsProcessorReadyForVmrun(VMCB* GuestVmcb, SEGMENT_ATTRIBUTE CsAttr);

enum VMEXIT_CODES {
    VMEXIT_CPUID = 0x72,
    VMEXIT_MSR = 0x7C,
    VMEXIT_VMRUN = 0x80,
    VMEXIT_VMMCALL = 0x81,
    VMEXIT_NPF = 0x400,
    VMEXIT_PF = 0x4E,
    VMEXIT_INVALID = -1,
    VMEXIT_GP = 0x4D,
    VMEXIT_DB = 0x41,
};

void InjectException(VPROCESSOR_DATA* Vpdata, int vector, int ErrorCode = 0)
{
    EVENTINJ EventInj;

    EventInj.Vector = vector;
    EventInj.Type = 3;
    EventInj.Valid = 1;

    if (ErrorCode != 0) {
        EventInj.PushErrorCode = 1;
        EventInj.ErrorCode = ErrorCode;
    }

    Vpdata->GuestVmcb.ControlArea.EventInj = EventInj.Flags;
}

ULONG64 GuestRip = 0;
void HandleNestedPageFault(VPROCESSOR_DATA* VpData, GUEST_REGISTERS* GuestContext)
{
    NPF_EXITINFO1 ExitInfo1;

    ExitInfo1.AsUInt64 = VpData->GuestVmcb.ControlArea.ExitInfo1;

    ULONG64 FailAddress = VpData->GuestVmcb.ControlArea.ExitInfo2;

    PHYSICAL_ADDRESS NCr3;

    NCr3.QuadPart = VpData->GuestVmcb.ControlArea.NCr3;

    GuestRip = VpData->GuestVmcb.SaveStateArea.Rip;


    if (ExitInfo1.Fields.Valid == 0) {

        int NumberOfBytes = VpData->GuestVmcb.ControlArea.NumOfBytesFetched;

        UINT8* InstructionBytes = VpData->GuestVmcb.ControlArea.GuestInstructionBytes;

        PML4E_64* Pml4Base = (PML4E_64*)MmGetVirtualForPhysical(NCr3);

        PTE_64* Pte64 = AssignNPTEntry((PML4E_64*)Pml4Base, FailAddress, true);

        return;
    }


    if (ExitInfo1.Fields.Execute == 1) {
        NPTHOOK_ENTRY* nptHook = GetHookByPhysicalPage(g_HvData, FailAddress);

        bool Switch = true;

        int length = 10;

        if (PAGE_ALIGN(GuestRip + length) != PAGE_ALIGN(GuestRip)) {

            PT_ENTRY_64* N_Pte = Utils::GetPte((PVOID)
                MmGetPhysicalAddress(PAGE_ALIGN(GuestRip)).QuadPart, NCr3.QuadPart);


            PT_ENTRY_64* N_Pte2 = Utils::GetPte((PVOID)
                MmGetPhysicalAddress(PAGE_ALIGN(GuestRip + length)).QuadPart, NCr3.QuadPart);


            N_Pte->ExecuteDisable = 0;
            N_Pte2->ExecuteDisable = 0;

            Switch = false;
        }   

        VpData->GuestVmcb.ControlArea.VmcbClean &= 0xFFFFFFEF;
        VpData->GuestVmcb.ControlArea.TlbControl = 3;

        /*  switch to hook CR3 or switch to innocent CR3   */
        if (Switch)
        {
            if (nptHook) {
                VpData->GuestVmcb.ControlArea.NCr3 = g_HvData->SecondaryNCr3;
            }
            else {
                VpData->GuestVmcb.ControlArea.NCr3 = g_HvData->PrimaryNCr3;
            }
        }
    }
}

void HandleCpuidExit(VPROCESSOR_DATA* VpData, GUEST_REGISTERS* GuestRegisters)
{
    VpData->GuestVmcb.SaveStateArea.Rip = VpData->GuestVmcb.ControlArea.NRip;
}

void HandleMsrExit(VPROCESSOR_DATA* VpData, GUEST_REGISTERS* GuestRegisters)
{
    VpData->GuestVmcb.SaveStateArea.Rip = VpData->GuestVmcb.ControlArea.NRip;
}


bool   HooksInitialized = false;

void HandleVmmcall(VPROCESSOR_DATA* VpData, GUEST_REGISTERS* GuestRegisters,
    bool* EndVM)
{
    int Registers[4];

    UINT64 Origin = GuestRegisters->Rdx;
    UINT64 Leaf = GuestRegisters->Rcx;

    if(Origin == KERNEL_VMMCALL)
    { 
        switch (Leaf) {

        case VMMCALL::HYPERVISOR_SIG: {
            Registers[0] = 'epyH'; /*	"HyperCheatzz"	*/
            Registers[1] = 'ehCr';
            Registers[2] = 'zzta';

            VpData->GuestVmcb.SaveStateArea.Rax = Registers[0];
            GuestRegisters->Rbx = Registers[1];
            GuestRegisters->Rcx = Registers[2];
            GuestRegisters->Rdx = Registers[3];

            break;
        }
        case VMMCALL::DISABLE_HOOKS: {
            VpData->GuestVmcb.ControlArea.NCr3 = g_HvData->TertiaryCr3;
            break;
        }
        case VMMCALL::ENABLE_HOOKS: {
            if (HooksInitialized == false)
            {
                InitializeHookList(g_HvData);

                SetHooks();
                KeInvalidateAllCaches();

                HooksInitialized = true;
            }
            else
            {
                VpData->GuestVmcb.ControlArea.NCr3 = g_HvData->PrimaryNCr3;
            }

            break;
        }
        case VMMCALL::END_HV: {
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
        g_HvData->HvCommand = Leaf;
    }
    VpData->GuestVmcb.SaveStateArea.Rip = VpData->GuestVmcb.ControlArea.NRip;
}

/*
        Vpdata = rcx
        Guest registers = rdx
*/
PDE_2MB_64* GuestQueryInfoFilePte = 0;
extern "C" bool
HandleVmexit(VPROCESSOR_DATA* VpData, GUEST_REGISTERS* GuestRegisters)
{
    /*	load host extra state	*/
    __svm_vmload(VpData->HostVmcbPa);

    bool EndVm = false;

    switch ((int)VpData->GuestVmcb.ControlArea.ExitCode) {

    case VMEXIT_CPUID: {
        HandleCpuidExit(VpData, GuestRegisters);
        break;
    }
    case VMEXIT_MSR: {
        HandleMsrExit(VpData, GuestRegisters);
        break;
    }
    case VMEXIT_VMRUN: {
        InjectException(VpData, 13);
        break;
    }
    case VMEXIT_VMMCALL: {
        HandleVmmcall(VpData, GuestRegisters, &EndVm);
        break;
    }
    case VMEXIT_PF: {
        DbgPrint("[VMEXIT] page fault occured at address %p \n",
            VpData->GuestVmcb.ControlArea.ExitInfo2);
        DbgPrint("[VMEXIT] guest RIP %p \n", VpData->GuestVmcb.SaveStateArea.Rip);
        break;
    }
    case VMEXIT_NPF: {
        HandleNestedPageFault(VpData, GuestRegisters);
        break;
    }
    case VMEXIT_DB: {
        DR6  dr6;
        dr6.Flags = VpData->GuestVmcb.SaveStateArea.Dr6;

        if (dr6.BreakpointCondition & 2 && IsInsideImage(VpData->GuestVmcb.SaveStateArea.Rip) == true)
        {
            logger->Log<ULONG64>("[BigBrother] dr1 address accessed from: HV_ANALYZED_IMAGE+0x%04x \n\n", VpData->GuestVmcb.SaveStateArea.Rip -
                (ULONG64)g_HvData->ImageStart, TYPES::pointer);

            logger->Log<ULONG64>("[BigBrother] accessed address %p \n", __readdr(1), TYPES::pointer);
            logger->LogCallStack((ULONG64*)GuestRegisters->Rbp, (ULONG64*)VpData->GuestVmcb.SaveStateArea.Rsp);
        }

        break;
    }
    case VMEXIT_GP: {
        char InstructionBytes[16] = { 0 };
        memcpy(InstructionBytes, (PVOID)VpData->GuestVmcb.SaveStateArea.Rip, 16);

        CR3 cr3;
        cr3.Flags = __readcr3();

        GuestQueryInfoFilePte = (PDE_2MB_64*)Utils::GetPte(
            NtQueryInformationFile, cr3.AddressOfPageDirectory);

        KeBugCheckEx(MANUALLY_INITIATED_CRASH,
            (ULONG64)InstructionBytes,
            (ULONG64)GuestRegisters,
            VpData->GuestVmcb.SaveStateArea.Rip,
            (ULONG64)GuestQueryInfoFilePte);

        InjectException(VpData, 13, 0xC0000005);
        break;
    }
    case VMEXIT_INVALID: {
        SEGMENT_ATTRIBUTE CsAttrib;
        CsAttrib.AsUInt16 = VpData->GuestVmcb.SaveStateArea.CsAttrib;

        IsProcessorReadyForVmrun(&VpData->GuestVmcb, CsAttrib);

        break;
    }
    default:
        DbgPrint("[VMEXIT] huh?? wtf why did I exit ?? exit code %p \n",
            VpData->GuestVmcb.ControlArea.ExitCode);
        break;
    }

    if (EndVm) {
        /*
                When we end the VM operation, we merge guest context with host context, and then jump to guest context.

                1. load guest state
                2. disable IF
                3. enable GIF
                4. disable SVME
                5. restore EFLAGS and re enable IF
                6. set RBX to RIP
                7. set RCX to RSP
                8. return and jump back
        */

        __svm_vmload(VpData->GuestVmcbPa);

        __svm_stgi();
        _disable();

        EFER_MSR Msr;

        Msr.Flags = __readmsr(AMD_EFER);
        Msr.SVME = 0;

        __writemsr(AMD_EFER, Msr.Flags);
        __writeeflags(VpData->GuestVmcb.SaveStateArea.Rflags);

        GuestRegisters->Rcx = VpData->GuestVmcb.SaveStateArea.Rsp;
        GuestRegisters->Rbx = VpData->GuestVmcb.ControlArea.NRip;
    }

    return EndVm;
}