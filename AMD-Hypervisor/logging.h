#pragma once
#include "includes.h"

namespace Logger 
{
	#define LOG_MAX_LEN 256

	//TRACELOGGING_DECLARE_PROVIDER(log_provider);

	NTSTATUS Start();

	void Log(const char* format, ...);

	void End();

};