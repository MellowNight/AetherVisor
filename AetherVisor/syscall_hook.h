#pragma once
#include "utils.h"

namespace SyscallHook
{
    bool EmulateSyscall(VcpuData* vcpu, GuestRegs guest_ctx);
    bool EmulateSysret(VcpuData* vcpu, GuestRegs guest_ctx);
};