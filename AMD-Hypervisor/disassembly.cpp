#pragma once
#include "disassembly.h"

namespace Disasm
{
	void HvRegContextToZydisRegContext(VcpuData* vcpu_data, GeneralRegisters* guest_regs, ZydisRegisterContext* context)
	{
		context->values[ZYDIS_REGISTER_RAX] = guest_regs->r10;
		context->values[ZYDIS_REGISTER_R10] = guest_regs->r10;
		context->values[ZYDIS_REGISTER_R10] = guest_regs->r10;
		context->values[ZYDIS_REGISTER_R10] = guest_regs->r10;
		context->values[ZYDIS_REGISTER_R10] = guest_regs->r10;
		context->values[ZYDIS_REGISTER_R10] = guest_regs->r10;
		context->values[ZYDIS_REGISTER_R10] = guest_regs->r10;
	}

	ZydisDecodedInstruction Disassemble(uint8_t* instruction, ZydisDecodedOperand* operands)
	{
		ZydisDecodedInstruction zydis_insn;

		ZydisDecoderDecodeFull(
			&zydis_decoder,
			instruction, 16,
			&zydis_insn,
			operands,
			ZYDIS_MAX_OPERAND_COUNT_VISIBLE,
			ZYDIS_DFLAG_VISIBLE_OPERANDS_ONLY
		);

		return zydis_insn;
	}

	/*	Gets total instructions length closest to byte_length	*/
	int	LengthOfInstructions(void* address, int byte_length)
	{
		ZydisDecodedOperand operands[ZYDIS_MAX_OPERAND_COUNT_VISIBLE];

		int insns_len = 0;
		for (insns_len = 0; insns_len < byte_length;)
		{
			int cur_insn_len = Disassemble((uint8_t*)address + insns_len, operands).length;
			insns_len += cur_insn_len;
		}

		return insns_len;
	}

	ZyanU64 GetCallJmpTarget(
		ZydisDecodedInstruction& instruction, 
		ZydisDecodedOperand* operands, 
		ZyanU64 runtime_address, 
		ZydisRegisterContext* registers
	)
	{
		auto destination = 0ULL;
		
		ZydisCalcAbsoluteAddressEx(&instruction, &operands[0], runtime_address, registers, &destination);

		return destination;
	}

	void ForEachInstruction(uint8_t* start, uint8_t* end, void(*Callback)(uint8_t* insn_addr, ZydisDecodedInstruction instruction))
	{
		size_t instruction_size = NULL;

		for (auto instruction = start; instruction < end; instruction = instruction + instruction_size)
		{
			ZydisDecodedOperand operands[ZYDIS_MAX_OPERAND_COUNT_VISIBLE];

			auto insn = Disasm::Disassemble(instruction, operands);

			Callback(instruction, insn);

			instruction_size = insn.length;
		}
	}

	int Init()
	{
		return ZydisDecoderInit(&zydis_decoder, ZYDIS_MACHINE_MODE_LONG_64, ZYDIS_STACK_WIDTH_64);
	}
};