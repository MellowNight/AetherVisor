#pragma once
#include "npt_hook.h"
#include "logging.h"
#include "prepare_vm.h"

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
    hook_efer_syscall = 0x1111111B,
    unbox_page = 0x1111111C,
    stop_branch_trace = 0x1111111D,
};

enum VMEXIT
{
    CPUID = 0x72,
    MSR = 0x7C,
    VMRUN = 0x80,
    VMMCALL = 0x81,
    NPF = 0x400,
    PF = 0x4E,
    UD = 0x46,
    BP = 0x43,
    INVALID = -1,
    GP = 0x4D,
    DB = 0x41,
    WRITE_CR3 = 0x13,
    DR0_READ = 0x20,
    DR6_READ = 0x26,
    DR7_READ = 0x27,
    PUSHF = 0x70,
    POPF = 0x71
};

extern "C" int __stdcall svm_vmmcall(VMMCALL_ID vmmcall_id, ...);
