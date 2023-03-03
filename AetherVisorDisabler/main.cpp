

#include    <ntifs.h>
#include    <Ntstrsafe.h>
#include    <intrin.h>
#include	<cstdint>

#include "aethervisor_kernel.h"

NTSTATUS DriverEntry(PDRIVER_OBJECT DriverObject, PUNICODE_STRING RegistryPath)
{
	AetherVisor::StopHv();

	return STATUS_SUCCESS;
}

int EntryPoint()
{
	DriverEntry(NULL, NULL);
	return 0;
}

