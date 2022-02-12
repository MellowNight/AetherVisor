#pragma once
#include "Global.h"

extern "C" int _fltused;

enum TYPES
{
	integer = 0,
	pointer = 1,
	floating = 2,
	none = 3,
};

struct LOG_ENTRY
{
	TYPES  argType;
	char message[120];
	char Data[8];
};

class LOGGER
{
private:
	BOOL InUse;
	int	EntryCount = 0;
	LOG_ENTRY* LogStack;

public:

	template <typename T>
	void Log(char* message, T data, TYPES argType)
	{
		while (InUse == 1)
		{
		}

		InUse = 1;
		strcpy(LogStack[EntryCount].message, message);

		if (argType != TYPES::none)
		{
			memcpy(LogStack[EntryCount].Data, &data, sizeof(T));
		}

		LogStack[EntryCount].argType = argType;

		EntryCount += 1; 
		InUse = 0;
	}

	void  LOGGER::LogCallStack(ULONG64* Rbp, ULONG64* Rsp);
	LOGGER*  LOGGER::Initialize();
	void  LOGGER::Flush();
};
extern LOGGER* logger;
