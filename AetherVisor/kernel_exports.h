#pragma once
#include "kernel_structures.h"

extern "C"
{
	PLIST_ENTRY PsLoadedModuleList;
	const char* PsGetProcessImageFileName(PEPROCESS Process);
    NTSYSAPI NTSTATUS NTAPI ZwQuerySystemInformation(
        IN int SystemInformationClass,
        OUT PVOID SystemInformation,
        IN ULONG SystemInformationLength,
        OUT PULONG ReturnLength OPTIONAL
    );
}