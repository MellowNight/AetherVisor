#include "logging.h"

namespace Logger
{
	//// GUID: e4536023-1c9d-4f87-b369-ef2b023bc280

	TRACELOGGING_DEFINE_PROVIDER(
		log_provider, 
		"TheHv",
		(0xe4536023, 0x1c9d, 0x4f87, 0xb3, 0x69, 0xef, 0x2b, 0x02, 0x3b, 0xc2, 0x80)
	);

	NTSTATUS Start()
	{
		return TraceLoggingRegister(log_provider);
	}

	void Log(const char* format, ...)
	{
		char buffer[LOG_MAX_LEN] = { 0 };

		va_list args;
		va_start(args, format);
		_vsnprintf(buffer, LOG_MAX_LEN - 1, format, args);
		va_end(args);

		TraceLoggingWrite(log_provider, "HV message", TraceLoggingString(buffer));
	}

	void End()
	{
		TraceLoggingUnregister( log_provider );
	}
};