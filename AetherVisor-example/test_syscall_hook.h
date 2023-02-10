#pragma once
#include "utils.h"

/*  test_syscall_hook.h:  Log process-wide syscalls   */

void SyscallHandler(GuestRegisters* registers, void* guest_rip)
{
    // TODO: use TLS variable to signal that a syscall hook is already being executed.

    
}

void EferSyscallHookTest()
{
    AetherVisor::SetCallback(AetherVisor::syscall, SyscallHandler);

    AetherVisor::SyscallHook::HookEFER();
}