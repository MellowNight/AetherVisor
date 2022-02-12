#pragma once
#include "global.h"

namespace Logger 
{
	struct LogEntry
	{
		size_t length;
		char buffer[256];
	};

	extern int log_entry_index;
	extern int log_max_entries;
	extern LogEntry* log_entries;

	void Init(int max_entries);
	void Log(const char* format, ...);
};