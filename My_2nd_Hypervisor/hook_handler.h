#pragma once
#include "Utility.h"
#include "Structs.h"

NTSTATUS NTAPI NtQueryInfoFile_handler(
    HANDLE FileHandle, 
    PIO_STATUS_BLOCK IoStatusBlock, 
    PVOID FileInformation,
	ULONG Length, 
    FILE_INFORMATION_CLASS FileInformationClass
);
extern NPTHOOK_ENTRY* NtQueryInfoFilehk_Entry;


extern NPTHOOK_ENTRY* KeStackAttachhk_Entry;
void KeStackAttach_handler(
    PRKPROCESS PROCESS, 
    PRKAPC_STATE ApcState
);


NTSTATUS __fastcall NtQuerySystemInfo_handler(
    IN SYSTEM_INFORMATION_CLASS SystemInformationClass,
    OUT PVOID SystemInformation,
    IN ULONG SystemInformationLength,
    OUT PULONG ReturnLength OPTIONAL
);
extern  NPTHOOK_ENTRY*  NtQuerySystemInfohk_entry;



NTSTATUS __fastcall MmCopyMemory_handler(
    _In_ PVOID TargetAddress,
    _In_ MM_COPY_ADDRESS SourceAddress,
    _In_ SIZE_T NumberOfBytes,
    _In_ ULONG Flags,
    _Out_ PSIZE_T NumberOfBytesTransferred
);
extern  NPTHOOK_ENTRY* MmCopyMemoryhk_entry;



PVOID NTAPI AllocPoolWithTag_handler(
    POOL_TYPE PoolType,
    SIZE_T NumberOfBytes,
    ULONG Tag
);
extern NPTHOOK_ENTRY* AllocPoolWithTaghk_entry;


NTSTATUS NTAPI NtOpenFile_handler(
    PHANDLE FileHandle,
    ACCESS_MASK DesiredAccess,
    POBJECT_ATTRIBUTES ObjectAttributes,
    PIO_STATUS_BLOCK IoStatusBlock,
    ULONG ShareAccess,
    ULONG OpenOptions
);
extern NPTHOOK_ENTRY* NtOpenFilehk_entry;



NTSTATUS NTAPI NtCreateFile_handler(
    PHANDLE FileHandle,
    ACCESS_MASK DesiredAccess,
    POBJECT_ATTRIBUTES ObjectAttributes,
    PIO_STATUS_BLOCK IoStatusBlock,
    PLARGE_INTEGER AllocationSize,
    ULONG FileAttributes,
    ULONG ShareAccess,
    ULONG CreateDisposition,
    ULONG CreateOptions,
    PVOID EaBuffer,
    ULONG EaLength
);
extern NPTHOOK_ENTRY* NtCreateFilehk_entry;


PVOID __stdcall RtlLookupElementGenericTableAvl_handler(
    _In_ PRTL_AVL_TABLE Table,
    _In_ PVOID Buffer
);
extern NPTHOOK_ENTRY* LookupElementGenericTableAvl_Entry;



extern PVOID   EacReadFile;

BOOL __fastcall EacReadFile_Handler(_UNICODE_STRING* FileName, __int64* pOutBuffer,
    ULONG* pFileLength, __int64 a4
);

extern NPTHOOK_ENTRY* EacReadFile_Entry;


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
);
extern NPTHOOK_ENTRY* NtDeviceIoctl_Entry;