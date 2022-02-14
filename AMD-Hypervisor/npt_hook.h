#pragma once
#include "utils.h"

/*	the stuff we will analyze	*/
#define		HV_ANALYZED_IMAGE	RTL_CONSTANT_STRING(L"EasyAntiCheat.sys")
#define		HV_ANALYZED_IMAGE2	RTL_CONSTANT_STRING(L"ModernWarfare.exe")

extern	wchar_t* HvAnalyzedImage2;


struct NptHookEntry
{
	PT_ENTRY_64* NptEntry1;
	PT_ENTRY_64* NptEntry2;
	int	OriginalInstrLen;
	union
	{
		struct Trampoline
		{
			char	OriginalBytes[20];
			char	jmp[6];
			void*	OriginalFunc;
		} Jmpout;
		char	Shellcode[64];
	};

	LIST_ENTRY	HookList;
};

NptHookEntry*	GetHookByPhysicalPage(GlobalHvData* HvData, UINT64 PagePhysical);
NptHookEntry*	GetHookByOldFuncAddress(GlobalHvData* HvData, void*	FuncAddr);
NptHookEntry*	AddHookedPage(GlobalHvData* HvData, void* PhysicalAddr, uintptr_t	NCr3, char* patch, int PatchLen);

void	SetHooks();
