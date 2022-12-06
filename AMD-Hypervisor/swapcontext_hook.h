#pragma once
#include "npthook_safety.h"
#include "portable_executable.h"
#include "vmexit.h"

namespace SwapContextHook
{
	extern Hooks::JmpRipCode ki_swap_context;

	__int64 __fastcall  ki_swap_context_hook(__int64 a1, PETHREAD ethread, __int64 a3);

	void Init();
};
