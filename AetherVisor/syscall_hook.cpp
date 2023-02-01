#include "syscall_hook.h"

namespace SyscallHook
{
    bool EmulateSyscall(GuestRegs Regs)
    {
        VMX_SEGMENT_SELECTOR Cs, Ss;
        UINT32               InstructionLength;
        UINT64               MsrValue;

        //
        // Reading guest's RIP
        //
        uintptr_t guest_rip = vcpu_data->guest_vmcb.save_state_area.rip;

        //
        // Reading instruction length
        //
        __vmx_vmread(VMCS_VMEXIT_INSTRUCTION_LENGTH, &InstructionLength);

        //
        // Reading guest's Rflags
        //
        //
        // Save the address of the instruction following SYSCALL into RCX and then
        // load RIP from IA32_LSTAR.
        //
        MsrValue  = __readmsr(IA32_LSTAR);
        Regs->rcx = guest_rip + insn_len;
        GuestRip  = MsrValue;
        __vmx_vmwrite(VMCS_GUEST_RIP, guest_rip);

        //
        // Save RFLAGS into R11 and then mask RFLAGS using IA32_FMASK
        //
        MsrValue  = __readmsr(IA32_FMASK);
        Regs->r11 = GuestRflags;
        GuestRflags &= ~(MsrValue | X86_FLAGS_RF);
        __vmx_vmwrite(VMCS_GUEST_RFLAGS, GuestRflags);

        //
        // Load the CS and SS selectors with values derived from bits 47:32 of IA32_STAR
        //
        MsrValue             = __readmsr(IA32_STAR);
        Cs.Selector          = (UINT16)((MsrValue >> 32) & ~3); // STAR[47:32] & ~RPL3
        Cs.Base              = 0;                               // flat segment
        Cs.Limit             = (UINT32)~0;                      // 4GB limit
        Cs.Attributes.AsUInt = 0xA09B;                          // L+DB+P+S+DPL0+Code
        SetGuestCs(&Cs);

        Ss.Selector          = (UINT16)(((MsrValue >> 32) & ~3) + 8); // STAR[47:32] + 8
        Ss.Base              = 0;                                     // flat segment
        Ss.Limit             = (UINT32)~0;                            // 4GB limit
        Ss.Attributes.AsUInt = 0xC093;                                // G+DB+P+S+DPL0+Data
        SetGuestSs(&Ss);

        return TRUE;
    }
};