#pragma once
#include "kernel_structures.h"

extern "C" __declspec(dllexport) PLIST_ENTRY PsLoadedModuleList;

extern "C"
{ 
    NTKERNELAPI
        PVOID
        NTAPI
        PsGetProcessWow64Process(IN PEPROCESS Process);

    NTKERNELAPI  PPEB NTAPI PsGetProcessPeb(
        IN PEPROCESS Process
    );
}