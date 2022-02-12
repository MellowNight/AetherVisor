#include "logging.h"
#include "npt_hook.h"
#include "hook_handler.h"

namespace Logger
{
	int log_entry_index;
	int log_max_entries;
	LogEntry* log_entries;

	void Init(int max_entries)
	{
		log_max_entries = max_entries;
		log_entry_index = 0;

		auto irql = KeGetCurrentIrql();

		/*	Make sure we aren't in vm root	*/
		NT_ASSERT(irql < DISPATCH_LEVEL);

		log_entries = (LogEntry*)ExAllocatePool(NonPagedPool, max_entries * sizeof(LogEntry));
	}

	void Log(const char* format, ...)
	{
		char buffer[256];
		va_list args;
		va_start(args, format);
		RtlStringCchPrintfA(buffer, 255, format, args);
		va_end(args);

		if (log_max_entries == log_entry_index)
		{
			/*	Wrap around and overwrite the log buffer when it's used up	*/
			log_entry_index = 0;
		}

		memcpy(log_entries[log_entry_index].buffer, buffer, 256);
	}
};