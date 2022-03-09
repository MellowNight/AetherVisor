#pragma once
#include "amd_definitions.h"

namespace Utils
{
	PVOID getDriverBaseAddress(OUT PULONG pSize, const char* driverName);

	int Diff(
		uintptr_t a,
		uintptr_t b
	);

	bool IsInsideRange(
		uintptr_t address, 
		uintptr_t range_base, 
		uintptr_t range_size
	);

	void* VirtualAddrFromPfn(
		uintptr_t pfn
	);

	PFN_NUMBER PfnFromVirtualAddr(
		uintptr_t va
	);

	/*	
		virtual_addr - virtual address to get pte of
		pml4_base_pa - base physical address of PML4
		page_table_callback - callback to call on the PTE, PDPTE, PDE, and PML4E associated 
		with this virtual address
	*/
	PT_ENTRY_64* GetPte(
		void* virtual_address,
		uintptr_t pml4_base_pa,
		int (*page_table_callback)(PT_ENTRY_64*) = NULL
	);


	PT_ENTRY_64* GetPte(
		void* virtual_address, 
		uintptr_t pml4_base_pa, 
		PDPTE_64** pdpte_result, 
		PDE_64** pde_result
	);

	PMDL LockPages(
		void* virtual_address, 
		LOCK_OPERATION  operation
	);

	NTSTATUS UnlockPages(PMDL mdl);

	void* GetDriverBaseAddress(
		size_t* out_driver_size,
		UNICODE_STRING driver_name
	);

	int Exponent(
		int base, 
		int power
	);

	HANDLE GetProcessId(
		const char* process_name
	);

    KIRQL DisableWP();

    void EnableWP(
		KIRQL tempirql
	);

	uintptr_t FindPattern(
		uintptr_t region_base, 
		size_t region_size, 
		const char* pattern, 
		size_t pattern_size, 
		char wildcard
	);
}