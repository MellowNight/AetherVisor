#pragma once
#include "amd_definitions.h"

#define RELATIVE_ADDR(insn, operand_offset, size) (ULONG64)(*(int*)((BYTE*)insn + operand_offset) + (BYTE*)insn + (int)size)

namespace Utils
{
	int ForEachCore(void(*callback)(void* params), void* params);

	int Diff(
		uintptr_t a,
		uintptr_t b
	);

	bool IsInsideRange(
		uintptr_t address, 
		uintptr_t range_base, 
		uintptr_t range_size
	);


	PVOID GetKernelModule(OUT PULONG pSize, UNICODE_STRING DriverName);

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