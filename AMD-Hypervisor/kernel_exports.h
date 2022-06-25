#pragma once
#include "kernel_structures.h"

extern "C" __declspec(dllexport) PLIST_ENTRY PsLoadedModuleList;


extern "C"
{
	const char* PsGetProcessImageFileName(PEPROCESS Process);
    NTSYSAPI NTSTATUS NTAPI ZwQuerySystemInformation(
        IN int SystemInformationClass,
        OUT PVOID SystemInformation,
        IN ULONG SystemInformationLength,
        OUT PULONG ReturnLength OPTIONAL
    );
}