#pragma once
#include "utils.h"
#include "hypervisor.h"
#include "npt.h"

namespace Instrumentation
{
    enum CALLBACK_ID
    {
        sandbox_readwrite = 0,
        sandbox_execute = 1,
        branch = 2,
        branch_trace_finished = 3,
        syscall = 4,
        max_id
    };


	extern void* callbacks[max_id];

    bool InvokeHook(VcpuData* vcpu, CALLBACK_ID handler, void* parameter = NULL, int params_size = NULL);
};

