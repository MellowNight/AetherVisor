#include "swapcontext_hook.h"
#include "npthook_safety.h"
#include "portable_executable.h"
#include "vmexit.h"
#include "branch_tracer.h"

namespace SwapContextHook
{
	Hooks::JmpRipCode ki_swap_context;

	__int64 __fastcall  ki_swap_context_hook(__int64 a1, PETHREAD ethread, __int64 a3)
	{
		auto result = static_cast<decltype(&ki_swap_context_hook)>(ki_swap_context.original_bytes)(a1, ethread, a3);

		if (BranchTracer::initialized && PsGetThreadId(ethread) == BranchTracer::thread_id && BranchTracer::thread_id)
		{
			//vDbgPrint("Switching to the traced thread! BranchTracer::thread_id %i \n", BranchTracer::thread_id);
			// Lock branch tracer thread and traced function thread to differeent cores using thread affinity	

		//	BranchTracer::Resume();
		}

		return result;
	}

	void Init()
	{
		/*	place a callback on NtTerminateProcess to remove npt hooks inside terminating processes, to prevent PFN check bsods	*/

		ULONG nt_size = NULL;

		auto ntoskrnl = (uintptr_t)Utils::GetKernelModule(&nt_size, RTL_CONSTANT_STRING(L"ntoskrnl.exe"));

		auto pe_hdr = PeHeader(ntoskrnl);

		auto section = (IMAGE_SECTION_HEADER*)(pe_hdr + 1);

		for (int i = 0; i < pe_hdr->FileHeader.NumberOfSections; ++i)
		{
			/*	MmCleanProcessAddressSpace hook to clean up NPT hooks after process exit	*/

			if (!strcmp((char*)section[i].Name, ".text"))
			{
				uint8_t* start = section[i].VirtualAddress + (uint8_t*)ntoskrnl;

				/*	IDA: "E8 ? ? ? ? 44 0F B6 F0 84 DB", "x????xxxxxx"	*/

				uintptr_t ki_swap_context_addr = Utils::FindPattern((uintptr_t)start, section[i].Misc.VirtualSize, "\xE8\xCC\xCC\xCC\xCC\x44\x0F\xB6\xF0\x84\xDB", 11, 0xCC);

				ki_swap_context_addr = RELATIVE_ADDR(ki_swap_context_addr, 1, 5);

				EASY_NPT_HOOK(Hooks::JmpRipCode, ki_swap_context, ki_swap_context_addr);
			}
		}
	}
};
