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
    void Init()
    {
        auto result = SymInitialize(
            GetCurrentProcess(), "srv*C:\\Symbols*https://msdl.microsoft.com/download/symbols", FALSE);
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

    uintptr_t LoadSymbolsForModule(std::string image_name, uintptr_t mapped_base, uintptr_t image_size)
    {
        auto result = SymLoadModuleEx(GetCurrentProcess(), NULL, image_name.c_str(), NULL, mapped_base, image_size, NULL, 0);
        return result;
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
            printf("SymFromAddr GetLastError %i \n", GetLastError());

            return std::string("");
        }

        return std::string(symbol_info.info.Name);
    }
};