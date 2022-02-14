#pragma once
#include "global.h"

namespace Logger 
{
	#define LOG_MAX_LEN 256

	TRACELOGGING_DECLARE_PROVIDER(log_provider);

	NTSTATUS Init();

	void Log(const char* format, ...);

	void LogSessionEnd();

};