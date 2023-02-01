#include "vmexit.h"

bool InvalidOpcodeHandler(VcpuData vcpu, GuestRegs* guest_ctx)
{
    auto guest_rip = vcpu->guest_vmcb.save_state_area.rip

    uintptr_t vmroot_cr3 = __readcr3();

    __writecr3(vcpu->guest_vmcb.save_state_area.cr3.Flags;);

    uint8_t insn_bytes[3] = { 0 };

    PageUtils::ReadPhysical(guest_rip, InstructionBuffer, 3);

	int rip_privilege = (guest_rip > 0x7FFFFFFFFFFF) ? 3 : 0;

    if (rip_privilege == 3 && insn_bytes[0] == 0x0F && insn_bytes[1] == 0x05)
    {
        SyscallHook::EmulateSyscall(guest_ctx, guest_rip);

        return true;
    }
    else if (rip_privilege == 0 && insn_bytes[0] == 0x48 && 
        insn_bytes[1] == 0x0F && insn_bytes[2] == 0x07)
    {
        SyscallHook::EmulateSysret(guest_ctx, guest_rip);

        return true;
    }

    __writecr3(vmroot_cr3);

    return false;
}

