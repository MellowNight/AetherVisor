#pragma once
#include "utils.h"

/* test_sandbox_hook.h: Catch out-of-module executes/reads/writes from our exe. */


/*	log out-of-module function calls and jmps		*/

void ExecuteHook(GuestRegisters* registers, void* return_address, void* o_guest_rip)
{
	AddressInfo retaddr_info = { return_address };
	AddressInfo rip_info = { o_guest_rip };

	std::cout << "[EXECUTE] \n";  
	std::cout << "return address = " << retaddr_info.Format() << "\n";

	std::cout << "RIP = %s \n" << rip_info.Format() << "\n\n";
}


/*	log specific reads and writes		*/

void ReadWriteHook(GuestRegisters* registers, void* o_guest_rip)
{
	AddressInfo rip_info = { o_guest_rip };

	ZydisDecodedOperand operands[5] = { 0 };

	auto instruction = Disasm::Disassemble((uint8_t*)o_guest_rip, operands);

	std::cout << "[READ/WRITE] \n";
    std::cout << "RIP = " << rip_info.Format() << "\n";

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

	Utils::Log("\n\n");
}

void SandboxTest()
{
    AetherVisor::SetCallback(AetherVisor::sandbox_readwrite, ReadWriteHook);
	AetherVisor::SetCallback(AetherVisor::sandbox_execute, ExecuteHook);

	//AetherVisor::Sandbox::SandboxRegion(module_base, PeHeader(module_base)->OptionalHeader.SizeOfImage);

	//AetherVisor::Sandbox::DenyRegionAccess((void*)Global::dll_params->dll_base, Global::dll_params->dll_size, false);
}