

extern ZydisDecoder ZyDec64;
ZyanU8 GetInstructionLength(ZyanU8* Instruction);

int	LengthOfInstructions(PVOID	address, int BytesLength);

ZydisDecoder ZyDec64;

ZyanU8 GetInstructionLength(ZyanU8* instruction)
{
	ZydisDecodedInstruction ZyIns;
	ZydisDecoderDecodeBuffer(&ZyDec64, instruction, 20, &ZyIns);
	return ZyIns.length;
}

/*	Gets total instructions length closest to byte_length	*/
int	LengthOfInstructions(void* address, int byte_length)
{
	int insns_len = 0;
	for (insns_len = 0; insn_len < byte_length;)
	{
		int cur_insn_len = GetInstructionLength((ZyanU8*)address + insn_len);
		insns_len += cur_insn_len;

		DbgPrint("CurInstructionLen %i \n", cur_insn_len);
	}
	DbgPrint("InstructionLen %i \n", insns_len);

	return insns_len;
}