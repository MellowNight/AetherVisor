#include "vmexit.h"
#include "syscall_hook.h"
#include "paging_utils.h"

bool InvalidOpcodeHandler(VcpuData* vcpu, GuestRegs* guest_ctx, PhysMemAccess* physical_mem)
{
    auto guest_rip = vcpu->guest_vmcb.save_state_area.rip;

    uintptr_t vmroot_cr3 = __readcr3();

    __writecr3(vcpu->guest_vmcb.save_state_area.cr3.Flags);

    uint8_t insn_bytes[3] = { 0 };

    physical_mem->ReadVirtual(guest_rip, insn_bytes, 3);

	int rip_privilege = (guest_rip > 0x7FFFFFFFFFFF) ? 3 : 0;

    if (rip_privilege == 3 && insn_bytes[0] == 0x0F && insn_bytes[1] == 0x05)
    {
        SyscallHook::EmulateSyscall(vcpu, guest_ctx);

        return true;
    }
    else if (rip_privilege == 0 && insn_bytes[0] == 0x48 && 
        insn_bytes[1] == 0x0F && insn_bytes[2] == 0x07)
    {
        SyscallHook::EmulateSysret(vcpu, guest_ctx);

        return true;
    }

    __writecr3(vmroot_cr3);

    return false;
}

