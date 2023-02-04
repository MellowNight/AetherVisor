#include "branch_trace.h"
#include "npt_hook.h"

namespace AetherVisor
{
    namespace BranchTracer
    {
        BranchLog* log_buffer;

        void TraceFunction(uint8_t* start_addr, uintptr_t range_base, uintptr_t range_size, uint8_t* stop_addr = NULL)
        {
            log_buffer = new BranchLog{};

            NptHook::SetNptHook((uintptr_t)start_addr, (uint8_t*)"\xCC", 1, sandbox);

            svm_vmmcall(VMMCALL_ID::start_branch_trace, start_addr, stop_addr, log_buffer, range_base, range_size);
        }
    }
}
