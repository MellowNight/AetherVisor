#include "Experiment.h"
#include "Pool.h"
#include "MapperTraces.h"



void	LaunchExperiments()
{
	uintptr_t text = Utils::GetSectionByName(g_HvData->ImageStart, ".text");

	DbgPrint("EAC text at %p \n", text);

	text += 0x50;

	for (int i = 0; i < CoreCount; ++i)
	{
		KAFFINITY	Affinity = Utils::ipow(2, i);

		KeSetSystemAffinityThread(Affinity);

		__writedr(1, text);

		DR7 dr7;
		dr7.Flags = __readdr(7);

		dr7.GlobalBreakpoint1 = 1;
		dr7.ReadWrite1 = 3;
		dr7.GlobalExactBreakpoint = 1;
		dr7.Length1 = 0; // 1 byte

		__writedr(7, dr7.Flags);
	}
}
