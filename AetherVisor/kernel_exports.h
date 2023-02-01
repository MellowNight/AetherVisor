#pragma once
#include "kernel_structures.h"

extern "C" __declspec(dllexport) PLIST_ENTRY PsLoadedModuleList;

extern "C"
{  
    NTKERNELAPI  PPEB NTAPI PsGetProcessPeb(
        IN PEPROCESS Process
    );
}