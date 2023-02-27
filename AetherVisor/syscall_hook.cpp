#include "syscall_hook.h"
#include "disassembly.h"
#include "instrumentation_hook.h"

using namespace Instrumentation;

namespace SyscallHook
{
    CR3 process_cr3;

    uintptr_t captured_rsp;
    uintptr_t captured_rip;
    uintptr_t captured_retaddr;

    struct TlsParams
    {
        bool callback_pending;
    };

    void Toggle(VcpuData* vcpu, bool intercept_syscalls)
    {
        process_cr3 = vcpu->guest_vmcb.save_state_area.cr3;

        vcpu->guest_vmcb.save_state_area.efer.syscall = !intercept_syscalls;
    }

    bool EmulateSysret(VcpuData* vcpu, GuestRegisters* guest_ctx)
    {
        // DbgPrint("EmulateSysret %p \n", EmulateSysret);

        //
        // SYSRET loads the CS and SS selectors with values derived from bits 63:48 of IA32_STAR
        //
        uintptr_t star_msr = __readmsr(IA32_STAR);

        vcpu->guest_vmcb.save_state_area.cs_selector = (uint16_t)(((star_msr >> 48) + 16) | 3); // (STAR[63:48]+16) | 3 (* RPL forced to 3 *)
        vcpu->guest_vmcb.save_state_area.cs_base = 0;               // Flat segment
        vcpu->guest_vmcb.save_state_area.cs_limit = UINT32_MAX;     // 4GB limit


        // L+DB+P+S+DPL3+Code

        vcpu->guest_vmcb.save_state_area.cs_attrib.fields.type = 0xB;
        vcpu->guest_vmcb.save_state_area.cs_attrib.fields.dpl = 3;
        vcpu->guest_vmcb.save_state_area.cs_attrib.fields.present = 1;
        vcpu->guest_vmcb.save_state_area.cs_attrib.fields.long_mode = 1;
        vcpu->guest_vmcb.save_state_area.cs_attrib.fields.system = 1;
        vcpu->guest_vmcb.save_state_area.cs_attrib.fields.granularity = 1;


        vcpu->guest_vmcb.save_state_area.ss_selector = (UINT16)(((star_msr >> 48) + 8) | 3); // (STAR[63:48]+8) | 3 (* RPL forced to 3 *)
        vcpu->guest_vmcb.save_state_area.ss_base = 0;               // Flat segment
        vcpu->guest_vmcb.save_state_area.ss_limit = UINT32_MAX;     // 4GB limit

        // G+DB+P+S+DPL3+Data

        vcpu->guest_vmcb.save_state_area.ss_attrib.fields.granularity = 1;
        vcpu->guest_vmcb.save_state_area.ss_attrib.fields.dpl = 3;
        vcpu->guest_vmcb.save_state_area.ss_attrib.fields.system = 1;
        vcpu->guest_vmcb.save_state_area.ss_attrib.fields.present = 1;
        vcpu->guest_vmcb.save_state_area.ss_attrib.fields.default_bit = 1;
        vcpu->guest_vmcb.save_state_area.ss_attrib.fields.type = 3;

        vcpu->guest_vmcb.save_state_area.cpl = 3;
        vcpu->guest_vmcb.save_state_area.rip = guest_ctx->rcx;

        //
        // Load RFLAGS from R11. Clear RF, VM, reserved bits
        //
        RFLAGS rflags; rflags.Flags = guest_ctx->r11;
        
        rflags.Virtual8086ModeFlag = 0;
        rflags.ResumeFlag = 0;
        rflags.Reserved1 = 0; rflags.Reserved2 = 0; rflags.Reserved3 = 0; rflags.Reserved4 = 0;
        rflags.ReadAs1 = 1;

        vcpu->guest_vmcb.save_state_area.rflags = rflags;

        return true;
    }

    bool EmulateSyscall(VcpuData* vcpu, GuestRegisters* guest_ctx)
    {
        uintptr_t guest_rip = vcpu->guest_vmcb.save_state_area.rip;

        if (vcpu->guest_vmcb.save_state_area.cr3.Flags == process_cr3.Flags)
        {
            /*  prevent infinite loops caused by syscalling from a syscall hook handler */

            auto tls_ptr = Utils::GetTlsPtr<TlsParams>(vcpu->guest_vmcb.save_state_area.gs_base, callbacks[syscall].tls_params_idx);

            DbgPrint("tls_index %i \n", callbacks[syscall].tls_params_idx);

            if (!(*tls_ptr)->callback_pending)
            {
                captured_rsp = vcpu->guest_vmcb.save_state_area.rsp;
                captured_rip = guest_rip;

                captured_retaddr = *(uintptr_t*)vcpu->guest_vmcb.save_state_area.rsp;

                (*tls_ptr)->callback_pending = TRUE;

                Instrumentation::InvokeHook(vcpu, Instrumentation::syscall);

                return false;
            }
            else if (
                vcpu->guest_vmcb.save_state_area.rsp == captured_rsp &&
                guest_rip == captured_rip &&
                captured_retaddr == *(uintptr_t*)vcpu->guest_vmcb.save_state_area.rsp)
            {
                DbgPrint("syscall hook finished \n");

                *tls_ptr = FALSE;
            }
        }

        ZydisDecodedOperand operands[5];

        auto insn_len = Disasm::Disassemble((uint8_t*)guest_rip, operands).length;

        //
        // Reading guest's Rflags
        //
        //
        // Save the address of the instruction following SYSCALL into RCX and then
        // load RIP from IA32_LSTAR.
        //
        auto lstar  = __readmsr(IA32_LSTAR);
        guest_ctx->rcx = guest_rip + insn_len;
        vcpu->guest_vmcb.save_state_area.rip = lstar;


        /*  Save RFLAGS into R11and then mask RFLAGS using IA32_FMASK   */

        auto fmask  = __readmsr(IA32_FMASK);

        guest_ctx->r11 = vcpu->guest_vmcb.save_state_area.rflags.Flags;

        vcpu->guest_vmcb.save_state_area.rflags.Flags &= ~(fmask | RFLAGS_RESUME_FLAG_BIT);

        /*  Load the CS and SS selectors with values derived from bits 47:32 of IA32_STAR   */

        auto star  = __readmsr(IA32_STAR);

        vcpu->guest_vmcb.save_state_area.cs_selector    = (uint16_t)((star >> 32) & ~3);   // STAR[47:32] & ~RPL3
        vcpu->guest_vmcb.save_state_area.cs_base        = 0;                               // flat segment
        vcpu->guest_vmcb.save_state_area.cs_limit       = UINT32_MAX;     // 4GB limit

        //  0xA9B, L+DB+P+S+DPL0+Code

        vcpu->guest_vmcb.save_state_area.cs_attrib.fields.type      = 0xB;
        vcpu->guest_vmcb.save_state_area.cs_attrib.fields.system    = 1;
        vcpu->guest_vmcb.save_state_area.cs_attrib.fields.dpl = 0;
        vcpu->guest_vmcb.save_state_area.cs_attrib.fields.present = 1;
        vcpu->guest_vmcb.save_state_area.cs_attrib.fields.long_mode = 1;
        vcpu->guest_vmcb.save_state_area.cs_attrib.fields.default_bit = 0;
        vcpu->guest_vmcb.save_state_area.cs_attrib.fields.granularity = 1;


        vcpu->guest_vmcb.save_state_area.ss_selector = (uint16_t)(((star >> 32) & ~3) + 8); // STAR[47:32] + 8
        vcpu->guest_vmcb.save_state_area.ss_base = 0;               // flat segment
        vcpu->guest_vmcb.save_state_area.ss_limit = UINT32_MAX;   // 4GB limit
                                                                                             
        //  G+DB+P+S+DPL0+Data

        vcpu->guest_vmcb.save_state_area.ss_attrib.fields.type = 3;
        vcpu->guest_vmcb.save_state_area.ss_attrib.fields.system = 1;
        vcpu->guest_vmcb.save_state_area.ss_attrib.fields.dpl = 0;
        vcpu->guest_vmcb.save_state_area.ss_attrib.fields.present = 1;
        vcpu->guest_vmcb.save_state_area.ss_attrib.fields.default_bit = 1;

        vcpu->guest_vmcb.save_state_area.ss_attrib.fields.granularity = 1;

        vcpu->guest_vmcb.save_state_area.cpl = 0;

        return true;
    }
};