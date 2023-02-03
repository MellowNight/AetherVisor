#pragma once
#include "utils.h"
#include "hypervisor.h"

namespace SyscallHook
{
    bool EmulateSyscall(VcpuData* vcpu, GuestRegisters* guest_ctx);
    bool EmulateSysret(VcpuData* vcpu, GuestRegisters* guest_ctx);

    void Init(VcpuData* vcpu, BOOLEAN EnableEFERSyscallHook);
};