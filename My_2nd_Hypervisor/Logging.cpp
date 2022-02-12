#include "Logging.h"
#include "npt_hook.h"
#include "hook_handler.h"

int _fltused = 0;

void  LOGGER::Flush()
{
	while (InUse == 1)
	{
	}
	InUse = 1;

	for (int i = 0; i < EntryCount; ++i)
	{
		switch (LogStack[i].argType)
		{
		case floating:
		{
			DbgPrint(LogStack[i].message, *(float*)LogStack[i].Data);
			break;
		}
		case integer:
		{
			DbgPrint(LogStack[i].message, *(int*)LogStack[i].Data);
			break;
		}
		case pointer:
		{
			DbgPrint(LogStack[i].message, *(ULONG64*)LogStack[i].Data);
			break;
		}
		case none:
		{
			DbgPrint(LogStack[i].message);
			break;
		}
		default:
			break;
		}
	}

	EntryCount = 0;
	InUse = 0;
}

void  LOGGER::LogCallStack(ULONG64* Rbp, ULONG64* Rsp)
{
	Log<int>("Callstack: ", NULL, TYPES::none);
	while (Rsp < Rbp)
	{
		DbgPrint("\t retaddr: %p \n", *Rsp);
		Rsp += 1;
	}
}

LOGGER*  LOGGER::Initialize()
{
	LOGGER* Logger = this;
	Logger = (LOGGER*)ExAllocatePool(NonPagedPool, sizeof(LOGGER));
	Logger->EntryCount = 0;
	Logger->InUse = 0;
	Logger->LogStack = (LOG_ENTRY*)ExAllocatePool(NonPagedPool, sizeof(LOG_ENTRY) * 300);
	RtlZeroMemory(Logger->LogStack, sizeof(LOG_ENTRY) * 300);
	
	return Logger;
}

LOGGER* logger = 0;