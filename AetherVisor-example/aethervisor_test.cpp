#include "shellcode.h"
#include "disassembly.h"
#include "dbg_symbols.h"
#include "aethervisor_test.h"

void StartTests()
{
	Disasm::Init();
	Symbols::Init();

	Utils::Log("Symbols::GetSymFromAddr((uintptr_t)GetProcAddress); %s \n", Symbols::GetSymFromAddr((uintptr_t)GetProcAddress).c_str());

	/*	Sandbox all BEService.exe pages 	*/

	uintptr_t beservice = NULL;

	while (!beservice)
	{
		Sleep(500);
		beservice = (uintptr_t)GetModuleHandle(L"BEService.exe");
	}

	SandboxTest();
	//BranchTraceTest();
	//NptHookTest();
	EferSyscallHookTest();
}