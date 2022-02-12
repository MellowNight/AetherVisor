#include "Pool.h"

bool FindPoolTable(PVOID* pPoolBigPageTable, PVOID* pPoolBigPageTableSize)
{
	PVOID	ntosBase = Utils::GetDriverBaseAddress(0, RTL_CONSTANT_STRING(L"ntoskrnl.exe"));


	PVOID ExProtectPoolExCallInstructionsAddress;
	Utils::BBScan(".text", (PCUCHAR)"\xE8\xCC\xCC\xCC\xCC\x83\x67\x0C\x00", '\xCC', 9, &ExProtectPoolExCallInstructionsAddress, ntosBase);


	if (!ExProtectPoolExCallInstructionsAddress)
		return false;

	PVOID ExProtectPoolExAddress = Utils::ResolveRelativeAddress(ExProtectPoolExCallInstructionsAddress, 1, 5);

	if (!ExProtectPoolExAddress)
		return false;

	PVOID PoolBigPageTableInstructionAddress = (PVOID)((ULONG64)ExProtectPoolExAddress + 0x95);
	*pPoolBigPageTable = Utils::ResolveRelativeAddress(PoolBigPageTableInstructionAddress, 3, 7);

	PVOID PoolBigPageTableSizeInstructionAddress = (PVOID)((ULONG64)ExProtectPoolExAddress + 0x8E);
	*pPoolBigPageTableSize = Utils::ResolveRelativeAddress(PoolBigPageTableSizeInstructionAddress, 3, 7);

	return true;
}

bool RemoveFromBigPool(PVOID Address)
{
	PVOID pPoolBigPageTable = 0;
	PVOID pPoolBigPageTableSize = 0;

	if (FindPoolTable(&pPoolBigPageTable, &pPoolBigPageTableSize))
	{
		PPOOL_TRACKER_BIG_PAGES PoolBigPageTable = *(PPOOL_TRACKER_BIG_PAGES*)pPoolBigPageTable;

		SIZE_T PoolBigPageTableSize = *(__int64*)pPoolBigPageTableSize;


		for (int i = 0; i < PoolBigPageTableSize; i++)
		{
			if (PoolBigPageTable[i].Va == (DWORD64)Address || PoolBigPageTable[i].Va == ((DWORD64)Address + 0x1))
			{
				PoolBigPageTable[i].Va = 0x1;
				PoolBigPageTable[i].NumberOfBytes = 0x0;


				DbgPrint("freed pool entry %p \n", Address);

				return true;
			}
		}

		return false;
	}

	return false;
}