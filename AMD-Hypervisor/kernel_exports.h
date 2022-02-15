#pragma once
#include "kernel_structures.h"

extern "C"
{
	LIST_ENTRY* PsLoadedModuleList;
	const char* PsGetProcessImageFileName(PEPROCESS Process);
}