#pragma once
#include "kernel_structures.h"

extern "C" __declspec(dllexport) PLIST_ENTRY PsLoadedModuleList;

extern "C"
{  
    NTKERNELAPI _IRQL_requires_(DISPATCH_LEVEL) _IRQL_requires_same_ VOID KeSignalCallDpcDone(_In_ PVOID SystemArgument1);

    NTKERNELAPI _IRQL_requires_(DISPATCH_LEVEL) _IRQL_requires_same_ LOGICAL KeSignalCallDpcSynchronize(_In_ PVOID SystemArgument2);

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