#include "logging.h"
#include "paging_utils.h"
#include "disassembly.h"
#include "portable_executable.h"
#include "vmexit.h"
#include "utils.h"
#include "npt_hook.h"
#include "instrumentation_hook.h"

namespace BranchTracer
{
	void Start(VcpuData* vcpu);
	void Stop(VcpuData* vcpu);

	void Pause(VcpuData* vcpu);
	void Resume(VcpuData* vcpu);

	void Init(
		VcpuData* vcpu, 
		uintptr_t start_addr, 
		uintptr_t stop_addr, 
		uintptr_t out_buffer,
		uintptr_t trace_range_base, 
		uintptr_t trace_range_size
	);

	extern CR3 process_cr3;

	extern bool active;	
	extern bool initialized;

	extern uintptr_t range_base;
	extern uintptr_t range_size;

	extern uintptr_t stop_address;
	extern uintptr_t start_address;

	extern HANDLE thread_id;

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

		void Log(VcpuData* vcpu, uintptr_t branch_address, uintptr_t target)
		{
			if (info.capacity - info.buffer_idx <= 5)
			{
				/*	notify to the guest that the branch tracing buffer is almost full	*/

				if (Instrumentation::InvokeHook(vcpu, Instrumentation::branch_log_full) == FALSE)
				{
				}
				else
				{
					/*	overwrite the buffer starting from the beginning...	*/

					info.buffer_idx = 0;

					return;
				}
			}

			info.buffer[info.buffer_idx].branch_target = target;
			info.buffer[info.buffer_idx].branch_address = branch_address;
			info.buffer_idx += 1;
		}
	};

	extern BranchLog* log_buffer;
};
