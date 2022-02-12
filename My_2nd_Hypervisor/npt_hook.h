#pragma once
#include "Utility.h"

/*	the stuff we will analyze	*/
#define		HV_ANALYZED_IMAGE	RTL_CONSTANT_STRING(L"EasyAntiCheat.sys")
#define		HV_ANALYZED_IMAGE2	RTL_CONSTANT_STRING(L"ModernWarfare.exe")

extern	wchar_t* HvAnalyzedImage2;


struct NPTHOOK_ENTRY
{
	PT_ENTRY_64* NptEntry1;
	PT_ENTRY_64* NptEntry2;
	int	OriginalInstrLen;
	union
	{
		struct Trampoline
		{
			char	OriginalBytes[20];
			char	Jmp[6];
			PVOID	OriginalFunc;
		} Jmpout;
		char	Shellcode[64];
	};

	LIST_ENTRY	HookList;
};

NPTHOOK_ENTRY*	GetHookByPhysicalPage(HYPERVISOR_DATA* HvData, UINT64 PagePhysical);
NPTHOOK_ENTRY*	GetHookByOldFuncAddress(HYPERVISOR_DATA* HvData, PVOID	FuncAddr);
NPTHOOK_ENTRY*	AddHookedPage(HYPERVISOR_DATA* HvData, PVOID PhysicalAddr, ULONG64	NCr3, char* patch, int PatchLen);


void	SetHooks();
