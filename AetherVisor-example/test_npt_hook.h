#pragma once
#include "utils.h"
#include "shellcode.h"

LONG WINAPI UdExceptionVEH(struct _EXCEPTION_POINTERS* ExceptionInfo)
{
    if (ExceptionInfo->ExceptionRecord->ExceptionCode == EXCEPTION_ILLEGAL_INSTRUCTION)
    {
        Aether::NptHook::Remove((uintptr_t)MessageBoxA);

        Sleep(1000);

        MessageBoxA(NULL, "This messagebox should be called!!!!!", "NPT hook test", MB_OK);

        auto rip = (uint8_t*)ExceptionInfo->ContextRecord->Rip;

        /*  force return from the patched messagebox */

        ExceptionInfo->ContextRecord->Rip = *(uintptr_t*)ExceptionInfo->ContextRecord->Rsp;

        ExceptionInfo->ContextRecord->Rsp += 8;

        return EXCEPTION_CONTINUE_EXECUTION;
    }

    // IT's not a break intruction. Continue searching for an exception handler.
    return EXCEPTION_CONTINUE_SEARCH;
}


/*  test_npt_hook.h:  Install a hidden NPT hook on user32.dll!MessageBoxA    */

void NptHookTest()
{
    auto veh = AddVectoredExceptionHandler(1, UdExceptionVEH);

    Aether::NptHook::Set((uintptr_t)MessageBoxA,
        (uint8_t*)"\x0F\x0B", 2, Aether::primary, true);

    MessageBoxA(NULL, "This messagebox shouldn't be called!!!!!", "NPT hook test", MB_OK);

    RemoveVectoredExceptionHandler(veh);

    MessageBoxA(NULL, "This messagebox 222 should be called!!!!!", "NPT hook test", MB_OK);
}