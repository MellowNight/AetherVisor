#pragma once
#include "global.h"
#include "Zydis/Zydis.h"

#define IMAGE_SCN_CNT_INITIALIZED_DATA  0x00000040
#define IMAGE_SCN_CNT_UNINITIALIZED_DATA    0x00000080
#define IMAGE_SCN_CNT_CODE  0x00000020
#define IMAGE_SCN_MEM_EXECUTE   0x20000000

namespace Utils
{
	bool IsInsideRange(
		uintptr_t address, 
		uintptr_t range_base, 
		uintptr_t range_size
	);

	void*	GetVaFromPfn(
		uintptr_t pfn
	);

	PFN_NUMBER	GetPfnFromVa(
		uintptr_t	Va
	);

	/*	
		virtual_addr - virtual address to get pte of
		pml4_base_pa - base physical address of PML4
		page_table_callback - callback to call on the PTE, PDPTE, PDE, and PML4E associated 
		with this virtual address
	*/
	PT_ENTRY_64* GetPte(
		void* virtual_addr, 
		uintptr_t pml4_base_pa, 
		int (*page_table_callback)(PT_ENTRY_64*) = NULL
	);

	PT_ENTRY_64* GetPte(
		void* virtual_addr, 
		uintptr_t pml4_base_pa, 
		PDPTE_64** PdpteResult, 
		PDE_64** PdeResult
	);

	void	GetJmpCode(uintptr_t jmpAddr, char* output);

	void*	GetSystemRoutineAddress(wchar_t* RoutineName, void** RoutinePhysical = NULL);

	PMDL	LockPages(void* VirtualAddress, LOCK_OPERATION  operation);
	NTSTATUS    UnlockPages(PMDL mdl);

	void* GetDriverBaseAddress(OUT PULONG pSize, UNICODE_STRING DriverName);

	uintptr_t GetSectionByName(void* base, const char* SectionNam);

	int ipow(int base, int power);

	HANDLE GetProcessID(const wchar_t* procName);

	NTSTATUS BBScan(IN PCCHAR section, IN PCUCHAR pattern, IN UCHAR wildcard, IN ULONG_PTR len, OUT void** ppFound, void* base = 0);
	NTSTATUS BBSearchPattern(IN PCUCHAR pattern, IN UCHAR wildcard, IN ULONG_PTR len, IN const VOID* base, IN ULONG_PTR size, OUT void** ppFound);
}