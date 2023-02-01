#include "vmexit.h"

bool InvalidOpcodeHandler(VcpuData vcpu_data, GuestRegs* guest_ctx)
{
    VIRTUAL_MACHINE_STATE * CurrentVmState = &g_GuestState[CoreIndex];

    auto guest_rip = vcpu_data->guest_vmcb.save_state_area.rip

    uintptr_t vmroot_cr3 = __readcr3();

    __writecr3(vcpu_data->guest_vmcb.save_state_area.cr3.Flags;);

    uint8_t InstructionBuffer[3] = {0};

    PageUtils::ReadPhysical(Rip, InstructionBuffer, 3);

    if ((guest_rip & 0xff00000000000000) && InstructionBuffer[0] == 0x0F && InstructionBuffer[1] == 0x05)
    {
        SyscallHook::EmulateSyscall(guest_ctx, guest_rip);

        return true;
    }
    else if (!(guest_rip & 0xff00000000000000) && InstructionBuffer[0] == 0x48 &&
        InstructionBuffer[1] == 0x0F &&
        InstructionBuffer[2] == 0x07)
    {
        SyscallHook::EmulateSysret(guest_ctx, guest_rip);

        return true;
    }

    __writecr3(OriginalCr3);


    return FALSE;
}

