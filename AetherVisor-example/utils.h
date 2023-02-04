#include "includes.h"

#define LOG_FILE "C:\\Users\\user123\\Desktop\\testing_drivers\\test_logs.txt"

namespace Utils
{
	bool NewFile(const char* file_path, const char* address, size_t size);

	void* ModuleFromAddress(uintptr_t address, PUNICODE_STRING out_name);

	void LogToFile(const char* file_name, const char* format, ...);

	void WriteToReadOnly(void* address, uint8_t* bytes, size_t len);

	void Log(const char* format, ...);

	size_t LoadFile(const wchar_t* path, char** buffer);

	bool IsAddressValid(void* address);
		
#pragma optimize( "", off )

	uintptr_t FindPattern(uintptr_t region_base, size_t region_size, const char* pattern, size_t pattern_size, char wildcard);

#pragma optimize( "", on )

}