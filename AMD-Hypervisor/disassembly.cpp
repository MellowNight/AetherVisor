#pragma once
#include "disassembly.h"

namespace Disasm
{
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

	ZyanU64 GetJmpTarget(ZydisDecodedInstruction& instruction, ZydisDecodedOperand* operands, ZyanU64 runtime_address)
	{
		auto destination = 0ULL;

		if ((operands[0].type == ZYDIS_OPERAND_TYPE_IMMEDIATE && operands[0].imm.is_relative == TRUE) || operands[0].type == ZYDIS_OPERAND_TYPE_MEMORY)
		{
			ZydisCalcAbsoluteAddress(&instruction, &operands[0], runtime_address, &destination);
		}

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