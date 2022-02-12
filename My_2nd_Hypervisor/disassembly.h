

extern ZydisDecoder ZyDec64;
ZyanU8 GetInstructionLength(ZyanU8* Instruction);

int	LengthOfInstructions(PVOID	address, int BytesLength);

ZydisDecoder ZyDec64;

ZyanU8 GetInstructionLength(ZyanU8* Instruction)
{
	ZydisDecodedInstruction ZyIns;
	ZydisDecoderDecodeBuffer(&ZyDec64, Instruction, 20, &ZyIns);
	return ZyIns.length;
}

/*	Gets total instructions length closest to BytesLength	*/
int	LengthOfInstructions(PVOID	address, int BytesLength)
{
	int InstructionLen = 0;
	for (InstructionLen = 0; InstructionLen < BytesLength;)
	{
		int CurInstructionLen = GetInstructionLength((ZyanU8*)address + InstructionLen);
		InstructionLen += CurInstructionLen;

		DbgPrint("CurInstructionLen %i \n", CurInstructionLen);
	}
	DbgPrint("InstructionLen %i \n", InstructionLen);

	return InstructionLen;
}