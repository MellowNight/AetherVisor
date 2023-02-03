#include "be_logging.h"
#include "hooks.h"
#include "logging.h"
#include "disassembly.h"
#include "dbg_symbols.h"

struct AddressInfo
{
	AddressInfo(void* _address) : address(_address)
	{
		symbol = Symbols::GetSymFromAddr((uintptr_t)address);

		dll_name_address.second = Utils::ModuleFromAddress((uintptr_t)address, &dll_name_address.first);
	}

	std::string Format()
	{
		char buffer[256];

		if (!symbol.empty())
		{
			Logger::Get()->Format(
				buffer, "%wZ!%s (0x%02x)", &dll_name_address.first, symbol.c_str(), address);
		}
		else if (dll_name_address.second)
		{
			Logger::Get()->Format(buffer, 
				"%wZ + 0x%02x", &dll_name_address.first, (uintptr_t)address - (uintptr_t)dll_name_address.second);
		}
		else
		{
			Logger::Get()->Format(buffer, "0x%02xn", address);
		}

		return std::string{ buffer };
	}



	void* address;
	std::pair<UNICODE_STRING, void*> dll_name_address;
	std::string	symbol;
};



void StartTests()
{
	Disasm::Init();
	Symbols::Init();

	Utils::Log("Symbols::GetSymFromAddr((uintptr_t)GetProcAddress); %s \n", Symbols::GetSymFromAddr((uintptr_t)GetProcAddress).c_str());

	/*	Sandbox all BEClient.dll pages 	*/

	uintptr_t beclient = NULL;

	while (!beclient)
	{
		Sleep(500);
		beclient = (uintptr_t)GetModuleHandle(L"BEClient.dll");
	}

	SandboxTest();
	BranchTraceTest();
	NptHookTest();
	EferSyscallHookTest();
}