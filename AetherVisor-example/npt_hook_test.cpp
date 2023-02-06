#include "aethervisor_test.h"
#include "shellcode.h"

/*  npt_hook_test.cpp:  Install a hidden NPT hook on kernel32.dll!GetProcAddress to catch BE's shellcode  */

#pragma comment(lib, "ntdll.lib")

enum MEMORY_INFORMATION_CLASS
{
    MemoryBasicInformation = 0
};

extern "C" NTSYSCALLAPI
NTSTATUS
NTAPI
NtQueryVirtualMemory(
    _In_ HANDLE ProcessHandle,
    _In_opt_ PVOID BaseAddress,
    _In_ MEMORY_INFORMATION_CLASS MemoryInformationClass,
    _Out_writes_bytes_(MemoryInformationLength) PVOID MemoryInformation,
    _In_ SIZE_T MemoryInformationLength,
    _Out_opt_ PSIZE_T ReturnLength
);


#define DUMP_PATH "C:\\Users\\user123\\Documents\\battleye\\shellcodes\\"

std::vector<uintptr_t> dumped_shellcodes;

WINBASEAPI FARPROC (WINAPI* get_proc_address)(
    HMODULE hModule, 
    LPCSTR lpProcName
);

Hooks::JmpRipCode* get_proc_address_hk;

LPVOID GetProcAddress_hk(HMODULE hModule, LPCSTR lpProcName) 
{
    uintptr_t retaddy = (uintptr_t)_ReturnAddress();

    MEMORY_BASIC_INFORMATION mbi{ 0 };

    size_t return_length{ 0 };

    if (NtQueryVirtualMemory((HANDLE)-1, (PVOID)retaddy, MemoryBasicInformation, &mbi, sizeof(mbi), &return_length) == 0) 
    {
        if (mbi.State == MEM_COMMIT && mbi.Protect == PAGE_EXECUTE_READWRITE && mbi.RegionSize > 0x1000)
        {
            if (std::find(dumped_shellcodes.begin(), dumped_shellcodes.end(), (uintptr_t)mbi.AllocationBase) == dumped_shellcodes.end()) 
            {
                std::string dump_path = DUMP_PATH + std::to_string((uintptr_t)mbi.BaseAddress) + ".dat";

                Utils::Log("Call from be-shellcode dumping: %s\n", dump_path.c_str());

                auto possible_shellcode_start = Utils::FindPattern(
                    (uintptr_t)mbi.BaseAddress, mbi.RegionSize, "\x4C\x89", 2, 0x00);

                Utils::Log("Possible entry-rva: %x\n", possible_shellcode_start - (uintptr_t)mbi.BaseAddress);

                Utils::NewFile(dump_path.c_str(), (char*)mbi.BaseAddress, mbi.RegionSize);

                dumped_shellcodes.push_back((uintptr_t)mbi.AllocationBase);
            }
        }
    }

    return static_cast<decltype(&GetProcAddress)>(get_proc_address_hk->original_bytes)(hModule, lpProcName);
}


void NptHookTest()
{
    get_proc_address_hk = new Hooks::JmpRipCode{ (uintptr_t)GetProcAddress, (uintptr_t)GetProcAddress_hk };

    AetherVisor::NptHook::Set(get_proc_address_hk->fn_address, 
        get_proc_address_hk->hook_code, get_proc_address_hk->hook_size, AetherVisor::primary, true);
}