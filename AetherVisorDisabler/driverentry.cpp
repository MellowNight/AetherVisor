#include    <ntifs.h>
#include    <Ntstrsafe.h>
#include	"aethervisor_kernel.h"

NTSTATUS DriverEntry(PDRIVER_OBJECT DriverObject, PUNICODE_STRING RegistryPath)
{
	AetherVisor::NptHook::Remove(NULL);

	return STATUS_SUCCESS;
}

void EntryPoint()
{
	DriverEntry(NULL, NULL);
}