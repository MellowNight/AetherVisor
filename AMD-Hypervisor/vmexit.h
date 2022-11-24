#pragma once
#include "npt_hook.h"
#include "logging.h"
#include "prepare_vm.h"

enum VMMCALL_ID : uintptr_t
{
    set_mpk_hook = 0x22FFAA1166,
    disable_hv = 0xFFAA221166,
    set_npt_hook = 0x6611AAFF22,
    remove_npt_hook = 0x1166AAFF22,
    is_hv_present = 0xEEFF,
    remap_page_ncr3_specific = 0x8236FF,
    sandbox_page = 0x8236FE,
    register_sandbox = 0x8F36FE,
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
};

void InjectException(VcpuData* core_data, int vector, bool push_error_code, int error_code);

extern "C" int __stdcall svm_vmmcall(VMMCALL_ID vmmcall_id, ...);

void HandleVmmcall(VcpuData* vmcb_data, GeneralRegisters* GuestRegisters, bool* EndVM);