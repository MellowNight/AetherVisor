#pragma once
#include "amd.h"

#define RELATIVE_ADDR(insn, operand_offset, size) (ULONG64)(*(int*)((BYTE*)insn + operand_offset) + (BYTE*)insn + (int)size)

namespace Utils
{
    void* GetUserModule32(PEPROCESS pProcess, PUNICODE_STRING ModuleName);

	void* GetKernelModule(size_t* out_size, UNICODE_STRING DriverName);

	int Exponent(
		int base, 
		int power
	);

	uintptr_t FindPattern(
		uintptr_t region_base, 
		size_t region_size, 
		const char* pattern, 
		size_t pattern_size, 
		char wildcard
	);

	void* PfnToVirtualAddr(
        uintptr_t pfn
    );

    PFN_NUMBER	VirtualAddrToPfn(
        uintptr_t va
    );

    PMDL LockPages(
        void* virtual_address,
        LOCK_OPERATION operation,
        KPROCESSOR_MODE access_mode,
        int size = PAGE_SIZE
    );

    NTSTATUS UnlockPages(
        PMDL mdl
    );

    int ForEachCore(void(*callback)(void* params), void* params);

    /*
        virtual_addr - virtual address to get pte of
        pml4_base_pa - base physical address of PML4
        page_table_callback - callback to call on the PTE, PDPTE, PDE, and PML4E associated
        with this virtual address
    */

    PT_ENTRY_64* GetPte(
        void* virtual_address, 
        uintptr_t pml4_base_pa,
        int (*page_table_callback)(PT_ENTRY_64*, void*) = NULL, 
        void* callback_data = NULL
    );

    void* GetUserModule(IN PEPROCESS pProcess, IN PUNICODE_STRING ModuleName, PPEB peb);
    
    template <typename T>
    T** GetTlsPtr(uintptr_t gs_base, uint32_t tls_index)
    {
        // gs_base == NtCurrentTeb(), OFFSETS ARE HARDCODED!!

        T** result = NULL;

        if (tls_index < 64)
        {
            result = (T**)(gs_base + 8 * tls_index + 0x1480);
        }
        else
        {
            auto tls_expansion_slots = *(uintptr_t*)(gs_base + 0x1780);

            result = (T**)(tls_expansion_slots + 8 * (tls_index - 0x40));
        }

        return result;
    }

    template <typename T>
    uint32_t Diff(T a, T b)
    {
        int diff = 0;

        if (a > b)
            diff = a - b;
        else
            diff = b - a;

        return diff;
    }
}