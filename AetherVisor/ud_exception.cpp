#include "vmexit.h"
#include "syscall_hook.h"
#include "paging_utils.h"

bool VcpuData::InvalidOpcodeHandler(GuestRegisters* guest_ctx, PhysMemAccess* physical_mem)
{
    DbgPrint("VcpuData::InvalidOpcodeHandler!! ! \n");

    auto guest_rip = guest_vmcb.save_state_area.rip;

    uintptr_t vmroot_cr3 = __readcr3();

    __writecr3(guest_vmcb.save_state_area.cr3.Flags);

    uint8_t insn_bytes[3] = { 0 };

    physical_mem->ReadVirtual((void*)guest_rip, insn_bytes, 3);

    int rip_privilege = (guest_rip > 0x7FFFFFFFFFFF) ? 3 : 0;

    if (rip_privilege == 3 && insn_bytes[0] == 0x0F && insn_bytes[1] == 0x05)
    {
        if (!tls_thread_is_handling_syscall)
        {
            tls_saved_rsp = rsp;
            tls_saved_rip = rip;
            tls_thread_is_handling_syscall = true;
            Instrumentation::InvokeHook(this, HOOK_ID::syscall);
        }
        else if (rsp == tls_saved_rsp && rip == tls_saved_rip)
        {
            tls_thread_is_handling_syscall = false;
        }
        
        SyscallHook::EmulateSyscall(this, guest_ctx);

        return true;
    }
    else if (rip_privilege == 0 && insn_bytes[0] == 0x48 && 
            insn_bytes[1] == 0x0F && insn_bytes[2] == 0x07) 
    {
        SyscallHook::EmulateSysret(this, guest_ctx);

        return true;
    }

    __writecr3(vmroot_cr3);

    return false;
}
