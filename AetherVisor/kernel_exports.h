#pragma once
#include "kernel_structures.h"

extern "C" __declspec(dllexport) PLIST_ENTRY PsLoadedModuleList;

extern "C"
{  
    NTKERNELAPI VOID KeGenericCallDpc(_In_ PKDEFERRED_ROUTINE Routine, _In_opt_ PVOID Context);

    NTKERNELAPI  PPEB NTAPI PsGetProcessPeb(
        IN PEPROCESS Process
    );

	const char* PsGetProcessImageFileName(
        PEPROCESS Process
    );

    NTSYSAPI NTSTATUS NTAPI ZwQuerySystemInformation(
        IN int SystemInformationClass,
        OUT PVOID SystemInformation,
        IN ULONG SystemInformationLength,
        OUT PULONG ReturnLength OPTIONAL
    );
}