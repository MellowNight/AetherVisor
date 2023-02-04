#pragma once
#include "utils.h"

namespace AetherVisor
{
    namespace BranchTracer
    {
        union BranchLog
        {
            struct LogEntry
            {
                uintptr_t branch_address;
                uintptr_t branch_target;
            };

            struct
            {
                int capacity;
                int buffer_idx;
                LogEntry* buffer;
            } info;

            LogEntry log_entries[PAGE_SIZE / sizeof(LogEntry)];

            BranchLog()
            {
                info.capacity = ((PAGE_SIZE - sizeof(info)) / sizeof(LogEntry));
                info.buffer_idx = 0;
                info.buffer = &log_entries[1];
            }
        };

        extern BranchLog* log_buffer;
    }
}
