#include "aethervisor_test.h"

void SyscallHandler(GuestRegisters* registers, void* guest_rip)
{
    // TODO: in the wrapper function, use TLS storage to signal that a syscall hook is already being executed.
}

void EferSyscallHookTest()
{
    AetherVisor::SetCallback(AetherVisor::syscall, SyscallHandler);

    AetherVisor::SyscallHook::HookEFER();
}