#include "vmexit.h"
#include "syscall_hook.h"

bool VcpuData::InvalidOpcodeHandler(GuestRegisters* guest_ctx)
{
    auto guest_rip = (uint8_t*)guest_vmcb.save_state_area.rip;

  //  DbgPrint("VcpuData::InvalidOpcodeHandler!! ! guest_rip %p \n", guest_rip);

    /*	page in the instruction's page if it's not present. */

    //if (!IsPagePresent(guest_rip))
    //{
    //    return false;
    //}

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
