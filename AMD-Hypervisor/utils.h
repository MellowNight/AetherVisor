#pragma once
#include "amd_definitions.h"

namespace Utils
{
	int Diff(
		uintptr_t a,
		uintptr_t b
	);

	bool IsInsideRange(
		uintptr_t address, 
		uintptr_t range_base, 
		uintptr_t range_size
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