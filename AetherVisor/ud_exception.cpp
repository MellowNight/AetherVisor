#include "vmexit.h"

bool InvalidOpcodeHandler(VcpuData vcpu_data, GuestRegs* guest_ctx)
{
    VIRTUAL_MACHINE_STATE * CurrentVmState = &g_GuestState[CoreIndex];

    auto guest_rip = vcpu_data->guest_vmcb.save_state_area.rip

    uintptr_t vmroot_cr3 = __readcr3();

    __writecr3(vcpu_data->guest_vmcb.save_state_area.cr3.Flags;);


    if (g_IsUnsafeSyscallOrSysretHandling)
    {
        //
        // In some computers, we realized that safe accessing to memory
        // is problematic. It means that our syscall approach might not
        // working properly.
        // Based on our tests, we realized that system doesn't generate #UD
        // regularly. So, we can imagine that only kernel addresses are SYSRET
        // instruction and SYSCALL is on a user-mode RIP.
        // It's faster than our safe methods but if the system generates a #UD
        // then a BSOD will happen. But if the system is working regularly, then
        // no BSOD happens. For more information, see documentation at !syscall2
        // or !sysret2 commands
        //
        if (guest_rip & 0xff00000000000000)
        {
            DispatchEventEferSysret(CoreIndex, guest_ctx, guest_rip);

            return true;
        }
        else
        {
            DispatchEventEferSyscall(CoreIndex, guest_ctx, guest_rip);

            return TRUE;
        }
    }
    else
    {
        //
        // No, longer needs to be checked because we're sticking to system process
        // and we have to change the cr3
        //
        // if ((GuestCr3.Flags & PCID_MASK) != PCID_NONE)

        //
        // Read the memory
        //
        UCHAR InstructionBuffer[3] = {0};

        if (MemoryMapperCheckIfPageIsPresentByCr3(Rip, GuestCr3))
        {
            //
            // The page is safe to read (present)
            // It's not necessary to use MemoryMapperReadMemorySafeOnTargetProcess
            // because we already switched to the process's cr3
            //
            MemoryMapperReadMemorySafe(Rip, InstructionBuffer, 3);
        }
        else
        {
            //
            // The page is not present, we have to inject a #PF
            //
            CurrentVmState->IncrementRip = FALSE;

            //
            // For testing purpose
            //
            // LogInfo("#PF Injected");

            //
            // Inject #PF
            //
            EventInjectPageFault(Rip);

            //
            // We should not inject #UD
            //
            return FALSE;
        }

        __writecr3(OriginalCr3);

        if (InstructionBuffer[0] == 0x0F &&
            InstructionBuffer[1] == 0x05)
        {
            goto EmulateSYSCALL;
        }

        if (InstructionBuffer[0] == 0x48 &&
            InstructionBuffer[1] == 0x0F &&
            InstructionBuffer[2] == 0x07)
        {
            goto EmulateSYSRET;
        }

        return FALSE;
    }
}

