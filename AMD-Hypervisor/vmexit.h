#pragma once
#include "itlb_hook.h"
#include "npt_hook.h"
#include "hv_interface.h"
#include "logging.h"
#include "prepare_vm.h"


enum VMEXIT
{
    CPUID = 0x72,
    MSR = 0x7C,
    VMRUN = 0x80,
    VMMCALL = 0x81,
    NPF = 0x400,
    PF = 0x4E,
    INVALID = -1,
    GP = 0x4D,
    DB = 0x41,
};

void InjectException(CoreVmcbData* core_data, int vector, int error_code = 0);