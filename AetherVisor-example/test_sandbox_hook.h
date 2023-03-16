#pragma once
#include "utils.h"

/* test_sandbox_hook.h: Catch out-of-module executes/reads/writes from our exe. */


void(*test_shellcode)(void* printf_addr, void* read_target) = static_cast<decltype(test_shellcode)>(
	(void*)
	"\x55"											// push	rbp
	"\x48\x89\xe5"									// mov	rbp, rsp
	"\x48\x83\xec\x08"								// sub	rsp, 64
	"\x48\x89\xCB"									// mov	rbx,rcx

	"\x48\x8B\x02"									// fuck it, mov rax, [rdx] 

	"\x48\x89\x14\x24"								// mov	QWORD PTR [rsp],rdx
	"\x48\xC7\xC0\x25\x73\x5C\x6E"					// mov	rax, 0x6e5c7325
	"\x50"											// push	rax
	"\x48\x89\xE1"									// mov	rcx,rsp
	"\x48\xB8\x72\x6C\x64\x21\x00\x00\x00\x00"		// movabs	rax,0x21646C72
	"\x50"											// push	rax
	"\x48\xB8\x48\x65\x6C\x6C\x6F\x20\x57\x6F"		// movabs	rax,0x6F57206F6C6C6548
	"\x50"											// push	rax
	"\x48\x89\xE2"				// mov	rdx,rsp
	"\x48\x83\xEC\x40"			// sub	rsp,0x40
	"\xFF\xD3"					// call	rbx
	"\x48\x83\xC4\x40"			// add	rsp,0x40
	"\x48\x83\xC4\x18"			// add	rsp,0x18
	"\x48\x8b\x14\x24"			// mov	rdx, QWORD PTR[rsp]
	"\x48\x8B\x02"				// mov	rax,QWORD PTR [rdx]
	"\x48\x89\xec"				// mov	rsp, rbp
	"\x5d"						// pop	rbp
	"\xc3"                      // ret

);

uint8_t padding_buffer[PAGE_SIZE] = { 0xCC };

/*	log out-of-module function calls and jmps		*/

void ExecuteHook(GuestRegisters* registers, void* return_address, void* o_guest_rip)
{
	Utils::ConsoleTextColor(FOREGROUND_RED | FOREGROUND_INTENSITY);

	AddressInfo retaddr_info = { return_address };
	AddressInfo rip_info = { o_guest_rip };

	std::cout << "\n[Aether::sandbox_execute] \nreturn address = " << retaddr_info.Format() << "\n";

	std::cout << "RIP = " << rip_info.Format() << "\n\n";
}


/*	log specific reads and writes		*/

void ReadWriteHook(GuestRegisters* registers, void* o_guest_rip)
{
	Utils::ConsoleTextColor(FOREGROUND_GREEN | FOREGROUND_INTENSITY);

	ZydisDecodedOperand operands[5] = { 0 };

	auto instruction = Disasm::Disassemble((uint8_t*)o_guest_rip, operands);

	std::cout << "[Aether::sandbox_readwrite] RIP = " << AddressInfo{ o_guest_rip }.Format() << "\n";

	ZydisRegisterContext context;

	Disasm::MyRegContextToZydisRegContext(registers, &context, o_guest_rip);

	for (int i = 0; i < instruction.operand_count_visible; ++i)
	{
		auto mem_target = Disasm::GetMemoryAccessTarget(
			instruction, &operands[i], (ZyanU64)o_guest_rip, &context);

		if (operands[i].actions & ZYDIS_OPERAND_ACTION_MASK_WRITE)
		{
			std::cout << "[write => 0x"<< std::hex << mem_target << "]" << std::endl;
		}
		else if (operands[i].actions & ZYDIS_OPERAND_ACTION_MASK_READ)
		{
			std::cout << "[read => 0x"<< std::hex << mem_target << "]" << std::endl;
		}
	}

	std::cout << "\n\n";
}

void SandboxTest()
{
	DWORD old_protect;

	VirtualProtect(test_shellcode, PAGE_SIZE, PAGE_EXECUTE_READWRITE, &old_protect);

	/*	intercept read access to our exe PE headers	*/

	Aether::SetCallback(Aether::sandbox_readwrite, ReadWriteHook);

	Aether::Sandbox::DenyRegionAccess((void*)GetModuleHandleA(NULL), PAGE_SIZE, false, true);

	/*	intercept the OutputDebugStringA API call	*/

	Aether::SetCallback(Aether::sandbox_execute, ExecuteHook);

	Aether::Sandbox::SandboxRegion((uintptr_t)test_shellcode, PAGE_SIZE);

	test_shellcode(OutputDebugStringA, GetModuleHandleA(NULL));

	Sleep(1000);

	Aether::Sandbox::UnboxRegion((uintptr_t)test_shellcode, PAGE_SIZE);
	Aether::Sandbox::UnboxRegion((uintptr_t)GetModuleHandleA(NULL), PAGE_SIZE);
}