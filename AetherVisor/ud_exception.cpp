#include "vmexit.h"
#include "syscall_hook.h"

bool VcpuData::InvalidOpcodeHandler(GuestRegisters* guest_ctx)
{
    auto guest_rip = (uint8_t*)guest_vmcb.save_state_area.rip;

  //  DbgPrint("VcpuData::InvalidOpcodeHandler!! ! guest_rip %p \n", guest_rip);

    /*	page in the instruction's page if it's not present. */

    auto guest_pte = Utils::GetPte((void*)guest_rip, __readcr3());

    if (guest_pte == NULL)
    {
        guest_vmcb.save_state_area.cr2 = (uintptr_t)guest_rip;

        InjectException(EXCEPTION_VECTOR::PageFault, true, guest_vmcb.control_area.exit_info1);

        // suppress_nrip_increment = true;

        return false;
    }

    int rip_privilege = ((uintptr_t)guest_rip < 0x7FFFFFFFFFFF) ? 3 : 0;

//   DbgPrint("rip_privilege %i \n", rip_privilege);
//    DbgPrint("guest_rip %p guest_rip[0] %p guest_rip[1] %p \n", guest_rip, guest_rip[0], guest_rip[1]);

    if (rip_privilege == 3 && guest_rip[0] == 0x0F && guest_rip[1] == 0x05)
    {
        SyscallHook::EmulateSyscall(this, guest_ctx);

        return true;
    }
    else if (rip_privilege == 0 && guest_rip[0] == 0x48 &&
        guest_rip[1] == 0x0F && guest_rip[2] == 0x07)
    {
        SyscallHook::EmulateSysret(this, guest_ctx);

        return true;
    }

    InjectException(EXCEPTION_VECTOR::InvalidOpcode, false, NULL);

    return false;
}
