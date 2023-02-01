#pragma once
#include "utils.h"

namespace SyscallHook
{
    bool EmulateSyscall(GuestRegs Regs);
};