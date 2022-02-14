#include	"npt_hook.h"

NptHookEntry* GetHookByPhysicalPage(GlobalHvData* HvData, UINT64 PagePhysical)
{
	PFN_NUMBER pfn = PagePhysical >> PAGE_SHIFT;

	NptHookEntry* nptHook;

	for (LIST_ENTRY* entry = HvData->hook_list_head; entry != 0; entry = entry->Flink) 
	{
		nptHook = CONTAINING_RECORD(entry, NptHookEntry, HookList);

		if (nptHook->NptEntry1) {

            if (nptHook->NptEntry1->PageFrameNumber == pfn) {
				return nptHook;
            }
        }
	}

	return 0;
}

NptHookEntry* AddHookedPage(GlobalHvData* HvData, void* PhysicalAddr, char* patch, int PatchLen, bool UseDisasm)
{
	int page_offset = (uintptr_t)PhysicalAddr & (PAGE_SIZE - 1);

	PT_ENTRY_64* InnocentNptEntry = Utils::GetPte(PhysicalAddr, HvData->primary_ncr3);

	PT_ENTRY_64* HookedNptEntry = Utils::GetPte(PhysicalAddr, HvData->secondary_ncr3);


	uintptr_t	CopyPage = (uintptr_t)ExAllocatePool(NonPagedPool, PAGE_SIZE);

	uintptr_t	PageAddr = (uintptr_t)Utils::VirtualAddrFromPfn(InnocentNptEntry->PageFrameNumber);

	if (!MmIsAddressValid((void*)PageAddr))
	{
		DbgPrint("Address invalid! PageAddr %p \n", PageAddr);
		DbgPrint("PhysicalAddr %p \n", PhysicalAddr);
		DbgPrint("nested page frame number %p \n", InnocentNptEntry->PageFrameNumber);
		return 0;
	}
	if (!MmIsAddressValid((void*)CopyPage))
	{
		DbgPrint("Address invalid! CopyPage %p \n", CopyPage);
		return 0;
	}

	memcpy((void*)CopyPage, (void*)PageAddr, PAGE_SIZE);

	memcpy((void*)(CopyPage + PageOffset), patch, PatchLen);

	InnocentNptEntry->ExecuteDisable = 1;

	HookedNptEntry->ExecuteDisable = 0;
	HookedNptEntry->PageFrameNumber = Utils::PfnFromVirtualAddr(CopyPage);


	LIST_ENTRY* entry = HvData->hook_list_head;

	while (MmIsAddressValid(entry->Flink))
	{
		entry = entry->Flink;
	}

	NptHookEntry* NewHook = (NptHookEntry*)ExAllocatePoolZero(NonPagedPool, sizeof(NptHookEntry), 'hook');

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
	
	memcpy(&NewHook->Jmpout.OriginalBytes, (void*)(PageAddr + PageOffset), NewHook->OriginalInstrLen);


	NewHook->NptEntry1 = InnocentNptEntry;
	NewHook->NptEntry2 = HookedNptEntry;

	return	NewHook;
}



char jmp[15];
void SetNptHook(
	void* address,
	void* hook_handler,
	bool execute_only,	// ONLY possible in usermode, with memory protection key
	uint8_t* hook_bytes
)
{
	NptHookEntry* hook_entry;

	void* HookedFunc = Function;
	void* HookedFuncPa = (void*)MmGetPhysicalAddress(HookedFunc).QuadPart;

	Utils::LockPages(HookedFunc, IoReadAccess);

	Utils::GetJmpCode((uintptr_t)hook_handler, jmp);

	hook_entry = AddHookedPage(global_hypervisor_data, HookedFuncPa, jmp, OriginalByteLen, UseDisasm);

	if (hook_entry)
	{
		Utils::GetJmpCode((uintptr_t)HookedFunc + (*HookEntry)->OriginalInstrLen,
			(uint8_t*)&(*HookEntry)->Jmpout.jmp);
	}
	else
	{
		/*	there was a problem	*/

		return;
	}

	DbgPrint("2nd page %p \n", Utils::VirtualAddrFromPfn((*HookEntry)->NptEntry2->PageFrameNumber));
	DbgPrint("original bytes shellcode %p \n", (*HookEntry)->Shellcode);
}

void SetHooks()
{
	SetNptHook(NtQueryInformationFile, NtQueryInfoFile_handler, &NtQueryInfoFilehk_Entry);
}