
#include "aethervisor_kernel.h"
#include "utils.h"


namespace Aether
{
    namespace BranchTracer
    {
        void BranchLog::Init()
        {
            info.capacity = ((PAGE_SIZE - sizeof(info)) / sizeof(LogEntry));
            info.buffer_idx = 0;
            info.buffer = &log_entries[1];
        }

        BranchLog* log_buffer;

        void Trace(uint8_t* start_addr, uintptr_t range_base, uintptr_t range_size, uint8_t* stop_addr)
        {
            log_buffer = (BranchLog*)ExAllocatePool(NonPagedPool, sizeof(BranchLog));

            log_buffer->Init();

            NptHook::Set((uintptr_t)start_addr, (uint8_t*)"\xCC", 1, sandbox);

            svm_vmmcall(VMMCALL_ID::start_branch_trace, start_addr, stop_addr, log_buffer, range_base, range_size);
        }
    }
}
