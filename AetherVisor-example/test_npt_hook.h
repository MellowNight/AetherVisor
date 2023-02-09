#pragma once
#include "utils.h"
#include "shellcode.h"

/*  test_npt_hook.h:  Install a hidden NPT hook on Wintrust.dll!WinVerifyTrust    */


Hooks::JmpRipCode* messagebox_hk;

int MessageBoxA_hk(HWND hWnd, LPCSTR lpText, LPCSTR lpCaption, UINT uType)
{
    uintptr_t retaddr = (uintptr_t)_ReturnAddress();

    std::cout << "MessageBoxA() called from 0x" << std::hex << retaddr << " lpText: " << lpText << "\n";

    std::cout << "First 8 bytes of MessageBoxA are 0x" << std::hex << *(uint64_t*)MessageBoxA;

    return static_cast<decltype(&MessageBoxA)>(messagebox_hk->original_bytes)(
        hWnd, lpText, lpCaption, uType);
}

void NptHookTest()
{
    messagebox_hk = new Hooks::JmpRipCode{ MessageBoxA, MessageBoxA_hk };

    AetherVisor::NptHook::Set(messagebox_hk->fn_address, 
        messagebox_hk->hook_code, messagebox_hk->hook_size, AetherVisor::primary, true);

    Sleep(1000);

    MessageBoxA(NULL, "HELLO WORLD!!!1111", "NPT hook test", MB_OK);
}