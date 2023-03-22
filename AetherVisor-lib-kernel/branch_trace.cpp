#include "aethervisor_kernel.h"
#include "utils.h"

namespace Aether
{
    namespace BranchTracer
    {
        extern "C"
        {
            /// BRANCH TRACER KERNEL INTERFACE TO BE DONE LATER!!!!!



           //std::vector<LogEntry> log_buffer;

           //TlsParams* tracer_params;

            void BranchCallbackInternal(GuestRegisters* registers, void* return_address, void* o_guest_rip)
            {
                /*      branch_callback(registers, return_address, o_guest_rip, tracer_params->last_branch_from);

                      if (log_buffer.size() < log_buffer.capacity())
                      {
                          log_buffer.push_back(LogEntry{ (uintptr_t)tracer_params->last_branch_from, (uintptr_t)o_guest_rip });
                      }
                      else
                      {
                          log_buffer.clear();
                      }*/
            }

            void Init()
            {
                //instrumentation_hooks[branch].tls_params_idx = TlsAlloc();

                //log_buffer.reserve(PAGE_SIZE / sizeof(LogEntry));

                //tracer_params = new TlsParams{ false, NULL };
            }

            void* Trace(uint8_t* start_addr, uintptr_t range_base, uintptr_t range_size, uint8_t* stop_addr)
            {
                //NptHook::Set((uintptr_t)start_addr, (uint8_t*)"\xCC", 1, primary);
                ////  NptHook::Set((uintptr_t)start_addr, (uint8_t*)"\xCC", 1, sandbox);

                //svm_vmmcall(VMMCALL_ID::start_branch_trace, start_addr, stop_addr, range_base, range_size, tracer_params);

                return NULL;
            }
        }
    }
}
