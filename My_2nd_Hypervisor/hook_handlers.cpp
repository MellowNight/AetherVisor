#include "hook_handler.h"
#include "npt_hook.h"
#include <ntifs.h>


UNICODE_STRING	TargetImage = HV_ANALYZED_IMAGE;


extern "C" POBJECT_TYPE* IoDeviceObjectType;
extern "C" __kernel_entry  NTSTATUS ZwQueryObject(
	HANDLE                   Handle,
	OBJECT_INFORMATION_CLASS ObjectInformationClass,
	PVOID                    ObjectInformation,
	ULONG                    ObjectInformationLength,
	PULONG                   ReturnLength
);

NPTHOOK_ENTRY* NtQueryInfoFilehk_Entry = 0;

NTSTATUS NTAPI NtQueryInfoFile_handler(HANDLE FileHandle, PIO_STATUS_BLOCK IoStatusBlock, PVOID FileInformation,
	ULONG Length, FILE_INFORMATION_CLASS FileInformationClass)
{
	/*	call original function	*/
	NTSTATUS status = reinterpret_cast<decltype(&NtQueryInfoFile_handler)>(&NtQueryInfoFilehk_Entry->Jmpout)(FileHandle,
		IoStatusBlock, FileInformation, Length, FileInformationClass);

	ULONG64	retaddr = *(ULONG64*)_AddressOfReturnAddress();

	if (strcmp(PsGetProcessImageFileName(PsGetCurrentProcess()), "ModernWarfare.exe") == 0)
	{
		DbgPrint("===========================================================================================================================\n");

		DbgPrint("[NtQueryInfoFile]  FileHandle: %p	IoStatusBlock: %p	FileInformation: %p	Length: %i	FileInformationClass: %i	\n",
			FileHandle, IoStatusBlock, FileInformation, Length, FileInformationClass);

		DbgPrint("[NtQueryInfoFile]  NtQueryInformationFile status:	%p \n", status);
		DbgPrint("[NtQueryInfoFile]  called from: %s \n\n", PsGetProcessImageFileName(PsGetCurrentProcess()));
	}

	return status;
}


NPTHOOK_ENTRY* KeStackAttachhk_Entry = 0;

void KeStackAttach_handler(PRKPROCESS PROCESS, PRKAPC_STATE ApcState)
{
	/*	call original	*/
	reinterpret_cast<decltype(&KeStackAttach_handler)>(&KeStackAttachhk_Entry->Jmpout)(PROCESS, ApcState);

	ULONG64	retaddr = *(ULONG64*)_AddressOfReturnAddress();

	if (IsInsideImage(retaddr) == true)
	{
		DbgPrint("===========================================================================================================================\n");

		DbgPrint("[KeStackAttach]  Process %s, ApcState	%p \n", PsGetProcessImageFileName(PROCESS), ApcState);

		DbgPrint("[KeStackAttach]  called from: %s \n\n", PsGetProcessImageFileName(PsGetCurrentProcess()));
	}
}



NPTHOOK_ENTRY* NtQuerySystemInfohk_entry = 0;

NTSTATUS NTAPI NtQuerySystemInfo_handler(
	IN SYSTEM_INFORMATION_CLASS SystemInformationClass, 
	OUT PVOID SystemInformation,
	IN ULONG SystemInformationLength,
	OUT PULONG ReturnLength OPTIONAL
)
{
	/*	call original	*/
	NTSTATUS status = reinterpret_cast<decltype(&NtQuerySystemInfo_handler)>(&NtQuerySystemInfohk_entry->Jmpout)(SystemInformationClass, SystemInformation,
		SystemInformationLength, ReturnLength);
	
	ULONG64	retaddr = *(ULONG64*)_AddressOfReturnAddress();

	if (strcmp(PsGetProcessImageFileName(PsGetCurrentProcess()), "ModernWarfare.exe") == 0)
	{
		DbgPrint("===========================================================================================================================\n");

		DbgPrint("[NtQuerySystemInfo]  SystemInformationClass: %i	SystemInformation: %p	SystemInformationLength: %i	ReturnLength: %p:	\n",
			SystemInformationClass, SystemInformation, SystemInformationLength, ReturnLength);

		DbgPrint("[NtQuerySystemInfo]  called from: %s \n\n", PsGetProcessImageFileName(PsGetCurrentProcess()));
	}

	return status;
}


NPTHOOK_ENTRY* MmCopyMemoryhk_entry = 0;

NTSTATUS __fastcall MmCopyMemory_handler(
	_In_ PVOID TargetAddress,
	_In_ MM_COPY_ADDRESS SourceAddress,
	_In_ SIZE_T NumberOfBytes,
	_In_ ULONG Flags,
	_Out_ PSIZE_T NumberOfBytesTransferred
)
{
	NTSTATUS status = reinterpret_cast<decltype(&MmCopyMemory_handler)>(&MmCopyMemoryhk_entry->Jmpout)(TargetAddress, SourceAddress,
		NumberOfBytes, Flags, NumberOfBytesTransferred);

	ULONG64	retaddr = *(ULONG64*)_AddressOfReturnAddress();

	if (IsInsideImage(retaddr) == true)
	{
		DbgPrint("===========================================================================================================================\n");

		DbgPrint("[MmCopyMemory]  TargetAddress: %p	SourceAddress: %p	NumberOfBytes: %i	Flags: %p NumberOfBytesTransferred %p: \n",
			TargetAddress, SourceAddress, NumberOfBytes, Flags, NumberOfBytesTransferred);

		DbgPrint("[MmCopyMemory]  called from: %wZ+0x%04x \n\n", TargetImage, retaddr - (ULONG64)g_HvData->ImageStart);
	}
	
	return status;
}



NPTHOOK_ENTRY* AllocPoolWithTaghk_entry = 0;

PVOID NTAPI AllocPoolWithTag_handler(
	POOL_TYPE PoolType,
	SIZE_T NumberOfBytes,
	ULONG Tag
)
{	
	/*	call original	*/
	PVOID result = reinterpret_cast<decltype(&AllocPoolWithTag_handler)>(&AllocPoolWithTaghk_entry->Jmpout)(PoolType, NumberOfBytes, Tag);

	ULONG64	retaddr = *(ULONG64*)_AddressOfReturnAddress();

	if (IsInsideImage(retaddr) == true)
	{
		DbgPrint("===========================================================================================================================\n");

		DbgPrint("[ExAllocatePoolWithTag]  PoolType: %i	NumberOfBytes: %i	Tag: %p Address %p: \n",
			PoolType, NumberOfBytes, Tag, result);

		DbgPrint("[ExAllocatePoolWithTag]  called from: HV_ANALYZED_IMAGE+0x%04x \n\n", TargetImage.Buffer, retaddr - (ULONG64)g_HvData->ImageStart);
	}

	return result;
}


NPTHOOK_ENTRY* NtCreateFilehk_entry = 0;

NTSTATUS NTAPI NtCreateFile_handler(PHANDLE FileHandle, ACCESS_MASK DesiredAccess, POBJECT_ATTRIBUTES ObjectAttributes,
	PIO_STATUS_BLOCK IoStatusBlock, PLARGE_INTEGER AllocationSize, ULONG FileAttributes, ULONG ShareAccess,
	ULONG CreateDisposition, ULONG CreateOptions, PVOID EaBuffer, ULONG EaLength
)
{
	/*	call original	*/
	NTSTATUS status = reinterpret_cast<decltype(&NtCreateFile_handler)>(&NtCreateFilehk_entry->Jmpout)(FileHandle, DesiredAccess, ObjectAttributes, IoStatusBlock,
		AllocationSize, FileAttributes, ShareAccess, CreateDisposition, CreateOptions, EaBuffer, EaLength);

	ULONG64	retaddr = *(ULONG64*)_AddressOfReturnAddress();

	if (strcmp(PsGetProcessImageFileName(PsGetCurrentProcess()), "ModernWarfare.exe") == 0)
	{
		DbgPrint("===========================================================================================================================\n");

		DbgPrint("[NtCreateFile]  FileHandle: %p	ObjectName: %wZ \n", FileHandle, ObjectAttributes->ObjectName);

		DbgPrint("[NtCreateFile]  called from: %s \n\n", PsGetProcessImageFileName(PsGetCurrentProcess()));
	}

	return status;
}




NPTHOOK_ENTRY* NtOpenFilehk_entry = 0;

NTSTATUS NTAPI NtOpenFile_handler(PHANDLE FileHandle, ACCESS_MASK DesiredAccess, POBJECT_ATTRIBUTES ObjectAttributes,
	PIO_STATUS_BLOCK IoStatusBlock, ULONG ShareAccess, ULONG OpenOptions)
{
	/*	call original	*/
	NTSTATUS status = reinterpret_cast<decltype(&NtOpenFile_handler)>(&NtOpenFilehk_entry->Jmpout)(FileHandle, DesiredAccess, 
		ObjectAttributes, IoStatusBlock, ShareAccess, OpenOptions);

	ULONG64	retaddr = *(ULONG64*)_AddressOfReturnAddress();

	if (strcmp(PsGetProcessImageFileName(PsGetCurrentProcess()), "ModernWarfare.exe") == 0)
	{
		DbgPrint("===========================================================================================================================\n");

		DbgPrint("[NtOpenFile]  FileHandle: %p	ObjectName: %wZ \n", FileHandle, ObjectAttributes->ObjectName);

		DbgPrint("[NtOpenFile]  called from: %s \n\n", PsGetProcessImageFileName(PsGetCurrentProcess()));
	}

	return status;
}

NPTHOOK_ENTRY* LookupElementGenericTableAvl_Entry = 0;

PVOID __stdcall RtlLookupElementGenericTableAvl_handler(
	_In_ PRTL_AVL_TABLE Table,
	_In_ PVOID Buffer
)
{
	/*	call original	*/
	PVOID result = reinterpret_cast<decltype(&RtlLookupElementGenericTableAvl_handler)>(&LookupElementGenericTableAvl_Entry->Jmpout)(Table, Buffer);

	ULONG64	retaddr = *(ULONG64*)_AddressOfReturnAddress();

	if (IsInsideImage(retaddr) == true)
	{
		DbgPrint("===========================================================================================================================\n");

		DbgPrint("[LookupElementGenericTableAvl]  Table: %p	Buffer: %p \n", Table, Buffer);

		DbgPrint("[LookupElementGenericTableAvl]  called from: HV_ANALYZED_IMAGE+0x%04x \n\n", retaddr - (ULONG64)g_HvData->ImageStart);
	}

	return result;
}

PVOID   EacReadFile;
NPTHOOK_ENTRY* EacReadFile_Entry = 0;

BOOL __fastcall EacReadFile_Handler(UNICODE_STRING* FileName, __int64* pOutBuffer, ULONG* pFileLength, __int64 a4)
{
	/*	call original	*/
	BOOL result = reinterpret_cast<decltype(&EacReadFile_Handler)>(&EacReadFile_Entry->Jmpout)(FileName, 
		pOutBuffer, pFileLength, a4);

	ULONG64	retaddr = *(ULONG64*)_AddressOfReturnAddress();

	DbgPrint("===========================================================================================================================\n");
	DbgPrint("[EacReadFile]  FileName: %wZ	Buffer: %p FileLength %i \n", FileName, *pOutBuffer, *pFileLength);
	DbgPrint("[EacReadFile]  called from: %wZ+0x%04x \n\n", TargetImage, retaddr - (ULONG64)g_HvData->ImageStart);

	return result;
}

NPTHOOK_ENTRY* NtDeviceIoctl_Entry = 0;
NTSTATUS NTAPI NtDeviceIoctl_Handler(
	HANDLE FileHandle,
	HANDLE Event,
	PIO_APC_ROUTINE ApcRoutine,
	PVOID ApcContext,
	PIO_STATUS_BLOCK IoStatusBlock,
	ULONG IoControlCode,
	PVOID InputBuffer,
	ULONG InputBufferLength,
	PVOID OutputBuffer,
	ULONG OutputBufferLength
)
{
	/*	call original	*/
	NTSTATUS status = reinterpret_cast<decltype(&NtDeviceIoctl_Handler)>(&NtDeviceIoctl_Entry->Jmpout)(FileHandle, Event,
		ApcRoutine, ApcContext, IoStatusBlock, IoControlCode, InputBuffer, InputBufferLength, OutputBuffer, OutputBufferLength);

	ULONG64	retaddr = *(ULONG64*)_AddressOfReturnAddress();

	if (strcmp(PsGetProcessImageFileName(PsGetCurrentProcess()), "ModernWarfare.exe") == 0)
	{
		ULONG			returnLength;
		PFILE_OBJECT	FileObject;

		NTSTATUS QueryStatus = ObReferenceObjectByHandle(FileHandle,
			GENERIC_READ, *IoFileObjectType, KernelMode, (PVOID*)&FileObject, NULL);

		POBJECT_NAME_INFORMATION NameInfo = (POBJECT_NAME_INFORMATION)ExAllocatePool(NonPagedPool, MAX_PATH);
		QueryStatus = ObQueryNameString(FileObject, NameInfo, MAX_PATH, &returnLength);
		
		if (NT_SUCCESS(QueryStatus))
		{
			DbgPrint("[NtDeviceIoctl] File name is %wZ \n", &NameInfo->Name);
		}
		else
		{
			DbgPrint("[NtDeviceIoctl] There was a problem getting File name, status %p \n", QueryStatus);
		}

		DbgPrint("[NtDeviceIoctl] IoControlCode: %p  \n\n", IoControlCode);
		DbgPrint("[NtDeviceIoctl]  called from: %s \n\n", PsGetProcessImageFileName(PsGetCurrentProcess()));
	}

	return status;
}