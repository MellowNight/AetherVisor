#pragma once
#include "amd.h"

#define RELATIVE_ADDR(insn, operand_offset, size) (ULONG64)(*(int*)((BYTE*)insn + operand_offset) + (BYTE*)insn + (int)size)

namespace Utils
{
	int ForEachCore(
		void(*callback)(void* params), 
		void* params
	);

	PVOID GetKernelModule(OUT PULONG pSize, UNICODE_STRING DriverName);

	int Exponent(
		int base, 
		int power
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