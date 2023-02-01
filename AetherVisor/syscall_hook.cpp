#include "syscall_hook.h"

namespace SyscallHook
{
    bool EmulateSysret(VcpuData* vcpu, GuestRegs guest_ctx)
    {
        SEGMENT_SELECTOR cs, ss;

        vcpu->guest_vmcb.save_state_area.rip = guest_ctx->rcx;

        //
        // Load RFLAGS from R11. Clear RF, VM, reserved bits
        //
        int64_t rflags = (guest_ctx->r11 & ~(X86_FLAGS_RF | X86_FLAGS_VM | X86_FLAGS_RESERVED_BITS)) | X86_FLAGS_FIXED;
        
        vcpu->guest_vmcb.save_state_area.rflags = rflags;

        //
        // SYSRET loads the CS and SS selectors with values derived from bits 63:48 of IA32_STAR
        //
        uintptr_t star_msr  = __readmsr(IA32_STAR);

        cs.Selector          = (uint16_t)(((star_msr >> 48) + 16) | 3); // (STAR[63:48]+16) | 3 (* RPL forced to 3 *)
        cs.Base              = 0;                                     // Flat segment
        cs.Limit             = (uint32_t)~0;                            // 4GB limit
        cs.Attributes.AsUInt = 0xA0FB;                                // L+DB+P+S+DPL3+Code
        SetGuestCs(&cs);

        ss.Selector          = (UINT16)(((star_msr >> 48) + 8) | 3); // (STAR[63:48]+8) | 3 (* RPL forced to 3 *)
        ss.Base              = 0;                                    // Flat segment
        ss.Limit             = (UINT32)~0;                           // 4GB limit
        ss.Attributes.AsUInt = 0xC0F3;                               // G+DB+P+S+DPL3+Data
        SetGuestSs(&ss);

        return TRUE;
    }

    bool EmulateSyscall(VcpuData* vcpu, GuestRegs guest_ctx)
    {
        SEGMENT_SELECTOR cs, ss;

        uintptr_t guest_rip = vcpu->guest_vmcb.save_state_area.rip;

        //
        // Reading guest's Rflags
        //
        //
        // Save the address of the instruction following SYSCALL into RCX and then
        // load RIP from IA32_LSTAR.
        //
        uintptr_t lstar  = __readmsr(IA32_LSTAR);
        guest_ctx->rcx = guest_rip + insn_len;
        vcpu->guest_vmcb.save_state_area.rip  = lstar;

        //
        // Save RFLAGS into R11 and then mask RFLAGS using IA32_FMASK
        //
        uintptr_t fmask  = __readmsr(IA32_FMASK);
        guest_ctx->r11 = vcpu->guest_vmcb.save_state_area.rflags;
        vcpu->guest_vmcb.save_state_area.rflags &= ~(fmask | X86_FLAGS_RF);

        //
        // Load the CS and SS selectors with values derived from bits 47:32 of IA32_STAR
        //
        uintptr_t star       = __readmsr(IA32_STAR);
        cs.Selector          = (uint16_t)((star >> 32) & ~3); // STAR[47:32] & ~RPL3
        cs.Base              = 0;                               // flat segment
        cs.Limit             = (uint32_t)~0;                      // 4GB limit
        cs.Attributes.AsUInt = 0xA09B;                          // L+DB+P+S+DPL0+Code
        SetGuestCs(&Cs);

        ss.Selector          = (uint16_t)(((star >> 32) & ~3) + 8); // STAR[47:32] + 8
        ss.Base              = 0;                                     // flat segment
        ss.Limit             = (uint32_t)~0;                            // 4GB limit
        ss.Attributes.AsUInt = 0xC093;                                // G+DB+P+S+DPL0+Data
        SetGuestSs(&ss);

        return true;
    }
};