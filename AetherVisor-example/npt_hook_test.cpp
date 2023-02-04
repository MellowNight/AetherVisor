#include "aethervisor_test.h"
#include "shellcode.h"

enum MEMORY_INFORMATION_CLASS
{
    MemoryBasicInformation = 0
};

extern "C" NTSTATUS NTAPI NtQueryVirtualMemory(
    _In_      HANDLE                   ProcessHandle,
    _In_opt_  PVOID                    BaseAddress,
    _In_      MEMORY_INFORMATION_CLASS MemoryInformationClass,
    _Out_     PVOID                    MemoryInformation,
    _In_      SIZE_T                   MemoryInformationLength,
    _Out_opt_ PSIZE_T                  ReturnLength
);

std::vector<uintptr_t> dumped_shellcodes;

WINBASEAPI FARPROC (WINAPI* get_proc_address)(HMODULE hModule, LPCSTR lpProcName);

Hooks::JmpRipCode get_proc_address_hk;

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
                std::string to_stream = "C:\\Users\\weak\\Desktop\\r6dmps\\shellcode\\" + std::to_string((uintptr_t)mbi.BaseAddress) + ".dat"; //R6 has no admin rights so dont use paths like C:\file.dat

                printf("Call from be-shellcode dumping: %s\n", to_stream.c_str());

                uintptr_t possible_shellcode_start = utils::scanpattern((uintptr_t)mbi.BaseAddress, mbi.RegionSize, "4C 89");

                printf("Possible entry-rva: %x\n", possible_shellcode_start - (uintptr_t)mbi.BaseAddress);

                utils::CreateFileFromMemory(to_stream, (char*)mbi.BaseAddress, mbi.RegionSize);

                dumped_shellcodes.push_back((uintptr_t)mbi.AllocationBase);
            }
        }
    }
    return get_proc_address(hModule, lpProcName);
}

/*	Install a hidden NPT hook on kernel32.dll!GetProcAddress to catch BE's shellcode  */

void NptHookTest()
{
    get_proc_address_hk = Hooks::JmpRipCode{ (uintptr_t)GetProcAddress, (uintptr_t)GetProcAddress_hk };

    AetherVisor::SetNptHook(
        get_proc_address_hk.fn_address, get_proc_address_hk.hook_code, get_proc_address_hk.hook_size, AetherVisor::primary, true);
}