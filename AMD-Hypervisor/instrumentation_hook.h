#pragma once
#include "utils.h"
#include "hypervisor.h"
#include "npt.h"

namespace Instrumentation
{
    enum HookId
    {
        sandbox_readwrite = 0,
        sandbox_execute = 1,
        branch_log_full = 2,
        branch_trace_finished = 3,
        max_id
    };

	extern void* callbacks[4];

    BOOL InvokeHook(VcpuData* vcpu_data, HookId handler, bool is_kernel);
};

