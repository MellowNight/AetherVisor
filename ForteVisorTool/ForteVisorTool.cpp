#include <iostream>
#include <Windows.h>
#include "forte_api.h"

void WriteToReadOnly(void* address, uint8_t* bytes, size_t len)
{
	DWORD old_prot;
	VirtualProtect((LPVOID)address, len, PAGE_EXECUTE_READWRITE, &old_prot);
	memcpy((void*)address, (void*)bytes, len);
	VirtualProtect((LPVOID)address, len, old_prot, 0);
}

int main()
{
	MessageBoxA(NULL, "aiojdaojdojeijfaef", "aodhaindsm", MB_OK);


	auto original_byte = *(uint8_t*)MessageBoxA;
	WriteToReadOnly(MessageBoxA, (uint8_t*)"\xC3", 1);
	WriteToReadOnly(MessageBoxA, &original_byte, 1);

	BVM::SetNptHook((uintptr_t)MessageBoxA, (uint8_t*)"\xC3", 1, 'test');

	MessageBoxA(NULL, "123", "12323", MB_OK);

	TerminateProcess(GetCurrentProcess(), 0x31);

	return 0;
}
