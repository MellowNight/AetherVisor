#include	"npt_hook.h"
#include	"hook_handler.h"
#include	"Structs.h"

NPTHOOK_ENTRY* GetHookByPhysicalPage(HYPERVISOR_DATA* HvData, UINT64 PagePhysical)
{
	PFN_NUMBER pfn = PagePhysical >> PAGE_SHIFT;

	NPTHOOK_ENTRY* nptHook;

	for (LIST_ENTRY* entry = HvData->HookListHead; entry != 0; entry = entry->Flink) 
	{
		nptHook = CONTAINING_RECORD(entry, NPTHOOK_ENTRY, HookList);

		if (nptHook->NptEntry1) {

            if (nptHook->NptEntry1->PageFrameNumber == pfn) {
				return nptHook;
            }
        }
	}

	return 0;
}


NPTHOOK_ENTRY* AddHookedPage(HYPERVISOR_DATA* HvData, PVOID PhysicalAddr, char* patch, int PatchLen, bool UseDisasm)
{
	int PageOffset = (ULONG64)PhysicalAddr & (PAGE_SIZE - 1);

	PT_ENTRY_64* InnocentNptEntry = Utils::GetPte(PhysicalAddr, HvData->PrimaryNCr3);

	PT_ENTRY_64* HookedNptEntry = Utils::GetPte(PhysicalAddr, HvData->SecondaryNCr3);


	ULONG64	CopyPage = (ULONG64)ExAllocatePool(NonPagedPool, PAGE_SIZE);

	ULONG64	PageAddr = (ULONG64)Utils::GetVaFromPfn(InnocentNptEntry->PageFrameNumber);

	if (!MmIsAddressValid((PVOID)PageAddr))
	{
		DbgPrint("Address invalid! PageAddr %p \n", PageAddr);
		DbgPrint("PhysicalAddr %p \n", PhysicalAddr);
		DbgPrint("nested page frame number %p \n", InnocentNptEntry->PageFrameNumber);
		return 0;
	}
	if (!MmIsAddressValid((PVOID)CopyPage))
	{
		DbgPrint("Address invalid! CopyPage %p \n", CopyPage);
		return 0;
	}

	memcpy((PVOID)CopyPage, (PVOID)PageAddr, PAGE_SIZE);

	memcpy((PVOID)(CopyPage + PageOffset), patch, PatchLen);

	InnocentNptEntry->ExecuteDisable = 1;

	HookedNptEntry->ExecuteDisable = 0;
	HookedNptEntry->PageFrameNumber = Utils::GetPfnFromVa(CopyPage);


	LIST_ENTRY* entry = HvData->HookListHead;

	while (MmIsAddressValid(entry->Flink))
	{
		entry = entry->Flink;
	}

	NPTHOOK_ENTRY* NewHook = (NPTHOOK_ENTRY*)ExAllocatePoolZero(NonPagedPool, sizeof(NPTHOOK_ENTRY), 'hook');

	entry->Flink = &NewHook->HookList;

	DbgPrint("InnocentNptEntry pageframe %p \n", InnocentNptEntry->PageFrameNumber);

	memset(&NewHook->Shellcode, '\x90', 64);

	/*	get original bytes length	*/

	if(UseDisasm)
	{
		NewHook->OriginalInstrLen = LengthOfInstructions((BYTE*)PageAddr + PageOffset, PatchLen);
	}
	else {
		NewHook->OriginalInstrLen = PatchLen;
	}
	
	memcpy(&NewHook->Jmpout.OriginalBytes, (PVOID)(PageAddr + PageOffset), NewHook->OriginalInstrLen);


	NewHook->NptEntry1 = InnocentNptEntry;
	NewHook->NptEntry2 = HookedNptEntry;

	return	NewHook;
}



char Jmp[15];
void SetNPTHook(PVOID Function, PVOID handler, NPTHOOK_ENTRY** HookEntry, bool UseDisasm = true, int OriginalByteLen = 15)
{
	PVOID	HookedFunc = Function;
	PVOID	HookedFuncPa = (PVOID)MmGetPhysicalAddress(HookedFunc).QuadPart;

	Utils::LockPages(HookedFunc, IoReadAccess);

	Utils::GetJmpCode((ULONG64)handler, Jmp);

	*HookEntry = AddHookedPage(g_HvData, HookedFuncPa, Jmp, OriginalByteLen, UseDisasm);

	if (*HookEntry)
	{
		Utils::GetJmpCode((ULONG64)HookedFunc + (*HookEntry)->OriginalInstrLen,
			(char*)&(*HookEntry)->Jmpout.Jmp);
	}
	else
	{
		/*	there was a problem	*/

		return;
	}

	DbgPrint("2nd page %p \n", Utils::GetVaFromPfn((*HookEntry)->NptEntry2->PageFrameNumber));
	DbgPrint("original bytes shellcode %p \n", (*HookEntry)->Shellcode);
}

void SetHooks()
{
	SetNPTHook(NtQueryInformationFile, NtQueryInfoFile_handler, &NtQueryInfoFilehk_Entry);
	//SetNPTHook(KeStackAttachProcess, KeStackAttach_handler, &KeStackAttachhk_Entry);
	SetNPTHook(NtQuerySystemInformation, NtQuerySystemInfo_handler, &NtQuerySystemInfohk_entry);
	//SetNPTHook(MmCopyMemory, MmCopyMemory_handler, &MmCopyMemoryhk_entry);
	//SetNPTHook(ExAllocatePoolWithTag, AllocPoolWithTag_handler, &AllocPoolWithTaghk_entry, false);
	SetNPTHook(NtCreateFile, NtCreateFile_handler, &NtCreateFilehk_entry, false, 14);
	SetNPTHook(NtOpenFile, NtOpenFile_handler, &NtOpenFilehk_entry, false, 17);
	//SetNPTHook(EacReadFile, EacReadFile_Handler, &EacReadFile_Entry, false, 14);
	SetNPTHook(NtDeviceIoControlFile, NtDeviceIoctl_Handler, &NtDeviceIoctl_Entry, false, 16);
}