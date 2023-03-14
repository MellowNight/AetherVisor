#pragma once
#include "includes.h"
#include "hypervisor.h"

namespace Disasm
{
	static ZydisDecoder zydis_decoder;

	ZydisDecodedInstruction Disassemble(
		uint8_t* instruction, 
		ZydisDecodedOperand* operands
	);

	/*	Gets total instructions length closest to byte_length	*/

	int	LengthOfInstructions(
		void* address, 
		int byte_length
	);

	void MyRegContextToZydisRegContext(
		VcpuData* vcpu, 
		GuestRegisters* guest_regs, 
		ZydisRegisterContext* context
	);

	void format(uintptr_t address, ZydisDecodedInstruction* instruction, char* out_buf, int size = 128);

	int Init();
};