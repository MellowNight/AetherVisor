#pragma once
#include "Utility.h"

#define MM_UNLOADED_DRIVERS_SIZE    50


typedef struct _MM_UNLOADED_DRIVER
{
	UNICODE_STRING 	    Name;
	PVOID 			    ModuleStart;
	PVOID 			    ModuleEnd;
	ULONG64 		    UnloadTime;
} MM_UNLOADED_DRIVER, * PMM_UNLOADED_DRIVER;

typedef struct PiDDBCacheEntry
{
	LIST_ENTRY		List;
	UNICODE_STRING	DriverName;
	ULONG			TimeDateStamp;
	NTSTATUS		LoadStatus;
	char			_0x0028[16]; // data from the shim engine, or uninitialized memory for custom drivers
}PIDCacheobj;


BOOLEAN IsUnloadedDriverEntryEmpty(
	_In_ PMM_UNLOADED_DRIVER Entry
)
{
	if (Entry->Name.MaximumLength == 0 ||
		Entry->Name.Length == 0 ||
		Entry->Name.Buffer == NULL)
	{
		return TRUE;
	}

	return FALSE;
}


UNICODE_STRING DriverName = RTL_CONSTANT_STRING(L"iqvw64e.sys");
UNICODE_STRING NewDriverName = RTL_CONSTANT_STRING(L"Yb97qgz.sys");



PMM_UNLOADED_DRIVER MmUnloadedDrivers;

PULONG			MmLastUnloadedDriver;

UCHAR			MmUnloadedDriverSig[] = "\x4C\x8B\x00\x00\x00\x00\x00\x4C\x8B\xC9\x4D\x85\x00\x74";
UCHAR			MmUnloadedDriverSig2[] = "\x48\x8B\x15\x00\x00\x00\x00\x48\x8B\xF1\x48\x85\xd2";

NTSTATUS findMMunloadedDrivers()
{
	PVOID MmUnloadedDriversPtr = NULL;

	Utils::BBScan("PAGE", MmUnloadedDriverSig, 0x00, sizeof(MmUnloadedDriverSig) - 1, (PVOID*)(&MmUnloadedDriversPtr));


	if (MmUnloadedDriversPtr == NULL) {
		DbgPrint("Unable to find MmUnloadedDriver sig, trying 2nd sig\n");

		Utils::BBScan("PAGE", MmUnloadedDriverSig2, 0x00, sizeof(MmUnloadedDriverSig2) - 1, (PVOID*)(&MmUnloadedDriversPtr));

		if (MmUnloadedDriversPtr == NULL)
		{
			DbgPrint("failed!\n");
		}
	}

	DbgPrint("MmUnloadedDriversPtr address found: %p  \n", MmUnloadedDriversPtr);


	MmUnloadedDrivers = *(PMM_UNLOADED_DRIVER*)Utils::ResolveRelativeAddress(MmUnloadedDriversPtr, 3, 7);

	DbgPrint("MmUnloadedDrivers real location is: %p\n", &MmUnloadedDrivers);

	return STATUS_SUCCESS;
}



UCHAR	 PiDDBLockPtr_sig[] = "\x48\x8D\x0D\x00\x00\x00\x00\xE8\x00\x00\x00\x00\x4C\x8B\x8C";

/*	48 83 25 ? ? ? ? 00 48 8D 0D ? ? ? ? 48 83 25 ? ? ? ? 00  48 83 25 ? ? ? ? 00
	+8 for PiddbLock instruction
*/
UCHAR	 PiDDBLockPtr_sig2[] = "\x48\x83\x25\xCC\xCC\xCC\xCC\x00\x48\x8D\x0D\xCC\xCC\xCC\xCC\xCC\x48\x83\x25\xCC\xCC\xCC\xCC\x00\x48\x83\x25\xCC\xCC\xCC\xCC\x00";


/*	48 8D 0D ? ? ? ? 48 8b d0 e8 ? ? ? ? 33 d2	*/
UCHAR	PiDDBCacheTablePtr_sig[] = "\x48\x8D\x0D\x00\x00\x00\x00\x48\x8b\xd0\xe8\x00\x00\x00\x00\x33\xd2";
UCHAR	PiDDBCacheTablePtr_sig2[] = "\x66\x03\xD2\x48\x8D\x0D";



bool LocatePiDDB(PERESOURCE* lock, PRTL_AVL_TABLE* table)
{
	PVOID PiDDBLockPtr = nullptr, PiDDBCacheTablePtr = nullptr;

	Utils::BBScan(".PAGE", PiDDBLockPtr_sig, 0, sizeof(PiDDBLockPtr_sig) - 1, reinterpret_cast<PVOID*>(&PiDDBLockPtr));


	if (PiDDBLockPtr != NULL)
	{
		DbgPrint("found PiDDBLockPtr sig. Piddblockptr is: %p\n", PiDDBLockPtr);
	}
	else
	{
		DbgPrint("PiDDBLockPtr not found, using second sig \n");


		Utils::BBScan(".PAGE", PiDDBLockPtr_sig2, 0xCC, sizeof(PiDDBLockPtr_sig2) - 1, reinterpret_cast<PVOID*>(&PiDDBLockPtr));

		PiDDBLockPtr = (PVOID64)((DWORD64)PiDDBLockPtr + 8);

		if (PiDDBLockPtr == NULL)
		{
			DbgPrint("failed to find PiDDBLock!!! \n");
		}
	}


	Utils::BBScan(".PAGE", PiDDBCacheTablePtr_sig, 0, sizeof(PiDDBCacheTablePtr_sig) - 1, reinterpret_cast<PVOID*>(&PiDDBCacheTablePtr));

	if (PiDDBCacheTablePtr != NULL)
	{
		DbgPrint("found PiDDBCacheTablePtr sig. PiDDBCacheTablePtr is: %p\n", PiDDBCacheTablePtr);
	}
	else
	{
		DbgPrint("failed to find PiDDBCacheTable, try 2nd sig \n");

		Utils::BBScan(".PAGE", PiDDBCacheTablePtr_sig2, 0, sizeof(PiDDBCacheTablePtr_sig2) - 1, reinterpret_cast<PVOID*>(&PiDDBCacheTablePtr));

		PiDDBCacheTablePtr = PVOID((uintptr_t)PiDDBCacheTablePtr + 3);
	}


	*lock = (PERESOURCE)(Utils::ResolveRelativeAddress(PiDDBLockPtr, 3, 7));

	*table = (PRTL_AVL_TABLE)(Utils::ResolveRelativeAddress(PiDDBCacheTablePtr, 3, 7));

	return true;
}





BOOLEAN ClearPiddbCacheTable()
{
	PERESOURCE PiDDBLock = NULL;
	PRTL_AVL_TABLE PiDDBCacheTable = NULL;

	NTSTATUS Status = LocatePiDDB(&PiDDBLock, &PiDDBCacheTable);


	if (PiDDBCacheTable == NULL || PiDDBLock == NULL)
	{
		DbgPrint("LocatePIDDB lock and/or cachetable not found\n");

		return Status;
	}
	else
	{
		DbgPrint("Successfully found PiddbCachetable and lock!!!1111\n");
		DbgPrint("PiddbLock: %p\n", PiDDBLock);
		DbgPrint("PiddbCacheTable: %p\n", PiDDBCacheTable);

		PIDCacheobj Entry;

		UNICODE_STRING DriverName = RTL_CONSTANT_STRING(L"iqvw64e.sys");

		Entry.DriverName = DriverName;

		Entry.TimeDateStamp = 0x5284EAC3;


		ExAcquireResourceExclusiveLite(PiDDBLock, TRUE);
		PIDCacheobj* pFoundEntry = (PIDCacheobj*)RtlLookupElementGenericTableAvl(PiDDBCacheTable, &Entry);


		if (pFoundEntry == NULL)
		{
			DbgPrint("pFoundEntry not found !!!\n");

			// release ddb resource lock
			ExReleaseResourceLite(PiDDBLock);

			return FALSE;
		}
		else
		{


			pFoundEntry->DriverName = NewDriverName;
			pFoundEntry->TimeDateStamp = 0x58891AC3;


			/*	DbgPrint("Found iqvw64e.sys in PiDDBCachetable!!\n");
				//unlink from list
				RemoveEntryList(&pFoundEntry->List);
				RtlDeleteElementGenericTableAvl(PiDDBCacheTable, pFoundEntry);
				// release the ddb resource lock	*/
			ExReleaseResourceLite(PiDDBLock);

			DbgPrint("Clear success and finish !!!\n");

			return TRUE;
		}

	}
}



BOOLEAN isMmUnloadedDriversFilled()
{
	PMM_UNLOADED_DRIVER entry;

	for (ULONG Index = 0; Index < MM_UNLOADED_DRIVERS_SIZE; ++Index)
	{
		entry = &MmUnloadedDrivers[Index];

		if (entry->Name.Buffer == NULL || entry->Name.Length == 0 || entry->Name.MaximumLength == 0)
		{
			return FALSE;
		}

	}
	return TRUE;
}




BOOLEAN cleanUnloadedDriverString()
{
	findMMunloadedDrivers();
	BOOLEAN cleared = FALSE;
	BOOLEAN Filled = isMmUnloadedDriversFilled();

	DbgPrint("about to clear mmunload\n");

	for (ULONG Index = 0; Index < MM_UNLOADED_DRIVERS_SIZE; ++Index)
	{
		PMM_UNLOADED_DRIVER Entry = &MmUnloadedDrivers[Index];

		if (RtlCompareUnicodeString(&DriverName, &Entry->Name, TRUE))
		{
			if (Index == 0)
			{
				DbgPrint("destroyed MmUnloaded Driver!!!\n");

				RtlZeroMemory(Entry, sizeof(MM_UNLOADED_DRIVER));
			}
			else
			{
				//random 7 letter name
				RtlCopyUnicodeString(&Entry->Name, &NewDriverName);
				Entry->UnloadTime = MmUnloadedDrivers[Index - 1].UnloadTime - 130;

				DbgPrint("DONE randomizing name inside CleanUnloadedDriverString\n");
			}
			return TRUE;
		}
	}
	DbgPrint("cannot find iqvw64e.sys!!!!111 cleanunloadeddriverstring fail!!1111\n");

	return FALSE;
}


BOOLEAN		clearKdmapperTraces()
{
	BOOLEAN		status;
	status = cleanUnloadedDriverString();
	if (status == FALSE)
	{
		DbgPrint("problem with mmunloadeddrivers\n");
	}
	status = ClearPiddbCacheTable();
	if (status == FALSE)
	{
		DbgPrint("problem with PiddbCacheTable\n");
	}
	return	 status;
}
