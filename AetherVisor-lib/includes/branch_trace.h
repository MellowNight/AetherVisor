#pragma once
#include  "aethervisor_base.h"

namespace AetherVisor
{
    namespace BranchTracer
    {
        extern "C" {

            void (*branch_log_full_handler)();
            void __stdcall branch_log_full_handler_wrap();

            void (*branch_trace_finish_handler)();
            void __stdcall branch_trace_finish_handler_wrap();
        }

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

        void Trace(uint8_t* start_addr, uintptr_t range_base, uintptr_t range_size, uint8_t* stop_addr = NULL);
    }
}
