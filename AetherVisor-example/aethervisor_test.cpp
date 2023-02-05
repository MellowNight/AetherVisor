#include "shellcode.h"
#include "disassembly.h"
#include "dbg_symbols.h"
#include "aethervisor_test.h"

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