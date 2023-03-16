#pragma once
#include "utils.h"

/*  test_syscall_hook.h:  Log process-wide syscalls   */

void SyscallHandler(GuestRegisters* registers, void* return_address, void* o_guest_rip)
{
    Utils::ConsoleTextColor(FOREGROUND_BLUE | FOREGROUND_INTENSITY);
    
    std::cout << "\n";

    std::stringstream stream;

    stream << std::hex << "[Aether::SyscallHook] - syscall index 0x" << registers->rax
        << " guest_rip 0x" << o_guest_rip
        << " return address 0x" << return_address 
        << std::endl;

    std::cout << stream.str();

    // TODO: use TLS variable to signal that a syscall hook is already being executed.
    // TODO: get this shit working on multiple cores.

    return;
}

void EferSyscallHookTest()
{
    Aether::SyscallHook::Init();

    Aether::SetCallback(Aether::syscall, SyscallHandler);

    Aether::SyscallHook::Enable();

    Sleep(1000);

    Aether::SyscallHook::Disable();
}