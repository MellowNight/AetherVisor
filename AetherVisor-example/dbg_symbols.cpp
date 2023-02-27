#include "dbg_symbols.h"
#include "portable_executable.h"
#include <DbgHelp.h>

struct PdbInfo
{
    uint32_t    magic;
    uint8_t     guid[16];
    uint32_t    age;
    char        pdb_name[1];
};

namespace Symbols
{
    uintptr_t LoadSymbolsForModule(std::string image_name, uintptr_t mapped_base, uintptr_t image_size)
    {
        auto result = SymLoadModuleEx(GetCurrentProcess(), NULL, image_name.c_str(), NULL, mapped_base, image_size, NULL, 0);
        
      //  std::cout << "SymLoadModuleEx GetLastError() = 0x" << std::hex << GetLastError() << " result 0x" << result << std::endl;

        return result;
    }

    void Init()
    {
        auto result = SymInitialize(GetCurrentProcess(),
            "srv*C:\\Symbols*https://msdl.microsoft.com/download/symbols", FALSE);

    #define LDR_IMAGESIZE 0x40
    #define BASE_DLL_NAME 0x58

		auto peb = (PPEB)__readgsqword(0x60);

		auto head = peb->Ldr->InMemoryOrderModuleList;

        auto curr = head;

		while (curr.Flink != head.Blink)
		{
			auto dll = (_LDR_DATA_TABLE_ENTRY*)CONTAINING_RECORD(curr.Flink, _LDR_DATA_TABLE_ENTRY, InMemoryOrderLinks);

            auto dll_size = ((uintptr_t)dll->DllBase + *(uintptr_t*)((uintptr_t)dll + LDR_IMAGESIZE));

			auto base_dll_name = *(PUNICODE_STRING)((uintptr_t)dll + BASE_DLL_NAME);

            std::wstring w_dll_name = base_dll_name.Buffer;

            LoadSymbolsForModule(
                std::string(w_dll_name.begin(), w_dll_name.end()), (uintptr_t)dll->DllBase, dll_size);

			curr = *curr.Flink;
		}
    }

    uint32_t GetSymAddr(uint32_t index, uintptr_t module_base, bool* status)
    {
        uint32_t offset = 0;

        auto sym_status = SymGetTypeInfo(GetCurrentProcess(), module_base, index, TI_GET_OFFSET, &offset);

        if (status)
        {
            *status = sym_status;
        }

        return offset;
    }

    std::string GetSymFromAddr(uintptr_t addr)
    {
        struct
        {
            SYMBOL_INFO info;
            char name_buf[128];
        } symbol_info;

        symbol_info.info.SizeOfStruct = sizeof(symbol_info.info);
        symbol_info.info.MaxNameLen = sizeof(symbol_info.name_buf);
        
        auto result = SymFromAddr(GetCurrentProcess(), addr, NULL, &symbol_info.info);

        if (!result)
        {
        //    printf("SymFromAddr GetLastError %i \n", GetLastError());

            return "";
        }

        return std::string(symbol_info.info.Name);
    }
};