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
    register_sandbox = 0x11111117,
    deny_sandbox_reads = 0x11111118,
    start_branch_trace = 0x11111119,
};

enum VMEXIT
{
    CPUID = 0x72,
    MSR = 0x7C,
    VMRUN = 0x80,
    VMMCALL = 0x81,
    NPF = 0x400,
    PF = 0x4E,
    BP = 0x43,
    INVALID = -1,
    GP = 0x4D,
    DB = 0x41,
    VMEXIT_MWAIT_CONDITIONAL = 0x8C,
    VMEXIT_TR_WRITE = 0x6D,
};

void InjectException(
    VcpuData* core_data, 
    int vector, 
    bool push_error_code, 
    int error_code
);

extern "C" int __stdcall svm_vmmcall(VMMCALL_ID vmmcall_id, ...);

void HandleVmmcall(
    VcpuData* vmcb_data, 
    GeneralRegisters* GuestRegisters, 
    bool* EndVM
);