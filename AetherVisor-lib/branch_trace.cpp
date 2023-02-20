#include "aethervisor.h"
#include "utils.h"


namespace Aether
{
    namespace BranchTracer
    {
        std::vector<LogEntry> log_buffer;
        uint32_t tls_index;

        void BranchCallbackInternal(GuestRegisters* registers, void* return_address, void* o_guest_rip, void* LastBranchFromIP)
        {
            branch_callback(registers, return_address, o_guest_rip, LastBranchFromIP);

            if (log_buffer.size() < log_buffer.capacity())
            {
                log_buffer.push_back(LogEntry{ (uintptr_t)LastBranchFromIP, (uintptr_t)o_guest_rip });
            }
            else
            {
                log_buffer.clear();
            }
        }

        void Init()
        {
            tls_index = TlsAlloc();

            log_buffer.reserve(PAGE_SIZE / sizeof(LogEntry));
        }

        void* Trace(uint8_t* start_addr, uintptr_t range_base, uintptr_t range_size, uint8_t* stop_addr)
        {
            TlsSetValue(tls_index, NULL);

            NptHook::Set((uintptr_t)start_addr, (uint8_t*)"\xCC", 1, primary);
          //  NptHook::Set((uintptr_t)start_addr, (uint8_t*)"\xCC", 1, sandbox);

            svm_vmmcall(VMMCALL_ID::start_branch_trace, start_addr, stop_addr, range_base, range_size, tls_index);

            return NULL;
        }
    }
}
