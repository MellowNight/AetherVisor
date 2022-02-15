#pragma once
#include "global.h"

namespace Logger 
{
	#define LOG_MAX_LEN 256

	TRACELOGGING_DECLARE_PROVIDER(log_provider);

	NTSTATUS Start();

	void Log(const wchar_t* format, ...);

	void End();

};