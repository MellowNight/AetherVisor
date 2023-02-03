#include "aethervisor_test.h"

/*	log out-of-module function calls and jmps		*/

void ExecuteHook(GuestRegisters* registers, void* return_address, void* o_guest_rip)
{
	AddressInfo retaddr_info = { return_address };
	AddressInfo rip_info = { o_guest_rip };

	Utils::Log("[EXECUTE]\n");  
	Utils::Log("return address = %s \n", retaddr_info.Format());

	Utils::Log("RIP = %s \n", rip_info.Format());

	Utils::Log("\n\n");
}


/*	log specific reads and writes		*/

void ReadWriteHook(GuestRegisters* registers, void* o_guest_rip)
{
	AddressInfo rip_info = { o_guest_rip };

	ZydisDecodedOperand operands[5] = { 0 };

	auto instruction = Disasm::Disassemble((uint8_t*)o_guest_rip, operands);

	Utils::Log("[READ/WRITE]\n");
    Utils::Log("RIP = %s \n", rip_info.Format());

	ZydisRegisterContext context;

	Disasm::MyRegContextToZydisRegContext(registers, &context, o_guest_rip);

	for (int i = 0; i < instruction.operand_count_visible; ++i)
	{
		auto mem_target = Disasm::GetMemoryAccessTarget(
			instruction, &operands[i], (ZyanU64)o_guest_rip, &context);

		if (operands[i].actions & ZYDIS_OPERAND_ACTION_MASK_WRITE)
		{
			Utils::Log("[write => 0x%02x]\n", mem_target);
		}
		else if (operands[i].actions & ZYDIS_OPERAND_ACTION_MASK_READ)
		{
			Utils::Log("[read => 0x%02x]\n", mem_target);
		}
	}

	Utils::Log("\n\n");
}

void SandboxTest()
{
    AetherVisor::InstrumentationHook(AetherVisor::sandbox_readwrite, ReadWriteHook);
	AetherVisor::InstrumentationHook(AetherVisor::sandbox_execute, ExecuteHook);

	AetherVisor::SandboxRegion(beclient, PeHeader(beclient)->OptionalHeader.SizeOfImage);

	auto kernel32 = (uint8_t*)GetModuleHandleA("kernel32.dll");

	AetherVisor::DenySandboxMemAccess(kernel32 + 0x1005);
}