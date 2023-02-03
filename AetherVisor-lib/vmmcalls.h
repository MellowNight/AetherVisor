#pragma once
#include <cstdint>
#include <Windows.h>
#include <math.h>
#include <intrin.h>

enum VMMCALL_ID : uintptr_t
{
    disable_hv = 0x11111111,
    set_npt_hook = 0x11111112,
    remove_npt_hook = 0x11111113,
    is_hv_present = 0x11111114,
    sandbox_page = 0x11111116,
    instrumentation_hook = 0x11111117,
    deny_sandbox_reads = 0x11111118,
    start_branch_trace = 0x11111119,
    hook_efer_syscall = 0x1111111A,
};