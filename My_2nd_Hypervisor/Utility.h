#pragma once
#include "Global.h"
#include	"Zydis/Zydis.h"

extern ZydisDecoder ZyDec64;
ZyanU8 GetInstructionLength(ZyanU8* Instruction);

int	LengthOfInstructions(PVOID	address, int BytesLength);

typedef int (*PageTableOperation)(PT_ENTRY_64*);

#define IMAGE_SCN_CNT_INITIALIZED_DATA  0x00000040
#define IMAGE_SCN_CNT_UNINITIALIZED_DATA    0x00000080
#define IMAGE_SCN_CNT_CODE  0x00000020
#define IMAGE_SCN_MEM_EXECUTE   0x20000000

namespace Utils
{
	PVOID	GetVaFromPfn(ULONG64	pfn);
	PFN_NUMBER	GetPfnFromVa(ULONG64	Va);

	PT_ENTRY_64* GetPte(PVOID VirtualAddress, ULONG64 Pml4BasePa, PageTableOperation Operation = NULL);
	PT_ENTRY_64* GetPte(PVOID VirtualAddress, ULONG64 Pml4BasePa, PDPTE_64** PdpteResult, PDE_64** PdeResult);

	KIRQL	disableWP();
	void	enableWP(KIRQL tempirql);

	void	GetJmpCode(ULONG64 jmpAddr, char* output);

	PVOID	GetSystemRoutineAddress(wchar_t* RoutineName, PVOID* RoutinePhysical = NULL);

	PMDL	LockPages(PVOID VirtualAddress, LOCK_OPERATION  operation);
	NTSTATUS    UnlockPages(PMDL mdl);

	PVOID GetDriverBaseAddress(OUT PULONG pSize, UNICODE_STRING DriverName);

	uintptr_t GetSectionByName(PVOID base, const char* SectionNam);
	int ipow(int base, int power);
	HANDLE GetProcessID(const wchar_t* procName);

	PVOID ResolveRelativeAddress(_In_ PVOID Instruction, _In_ ULONG OffsetOffset, _In_ ULONG InstructionSize);
	void PrintModuleFromAddress(PVOID address);

	NTSTATUS BBScan(IN PCCHAR section, IN PCUCHAR pattern, IN UCHAR wildcard, IN ULONG_PTR len, OUT PVOID* ppFound, PVOID base = 0);
	NTSTATUS BBSearchPattern(IN PCUCHAR pattern, IN UCHAR wildcard, IN ULONG_PTR len, IN const VOID* base, IN ULONG_PTR size, OUT PVOID* ppFound);
}