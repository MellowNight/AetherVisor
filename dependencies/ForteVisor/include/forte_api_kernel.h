#pragma once
#include <ntddk.h>
#include <cstdint>

enum VMMCALL_ID : uintptr_t
{
    set_mpk_hook = 0x22FFAA1166,
    set_tlb_hook = 0xAAFF226611,
    disable_hv = 0xFFAA221166,
    set_npt_hook = 0x6611AAFF22,
};

extern "C" int __stdcall svm_vmmcall(VMMCALL_ID vmmcall_id, ...);

namespace ForteVisor
{
    int SetNptHook(uintptr_t address, uint8_t* patch, size_t patch_len);

    int ForEachCore(void(*callback)(void* params), void* params = NULL);

    int DisableHv();
};