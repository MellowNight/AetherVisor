#pragma once
#include "utils.h"

namespace SyscallHook
{
    bool EmulateSyscall(VcpuData* vcpu, GuestRegs Regs);
    bool EmulateSysret(VcpuData* vcpu, GuestRegs Regs);
};