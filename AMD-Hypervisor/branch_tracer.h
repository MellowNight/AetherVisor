#include "logging.h"
#include "paging_utils.h"
#include "disassembly.h"
#include "portable_executable.h"
#include "vmexit.h"
#include "utils.h"

namespace BranchTracer
{
	void Init(VcpuData* vcpu_data, uintptr_t start_addr, uintptr_t log_buffer, int log_buffer_size);

	void Resume(VcpuData* vcpu_data);
	void Stop(VcpuData* vcpu_data);
	void Resume();

	void Init(VcpuData* vcpu_data, uintptr_t start_addr, uintptr_t out_buffer);

	extern bool active;	
	extern bool initialized;

	extern uintptr_t start_address;
	extern HANDLE thread_id;

	struct BranchLog
	{
		int capacity;
		int buffer_idx;

		uintptr_t* buffer;

		void Log(uintptr_t entry)
		{
			if (buffer_idx < (capacity / sizeof(uintptr_t)))
			{
				buffer[buffer_idx] = entry;
				buffer_idx += 1;
			}
			else
			{
				/*	overwrite starting from the beginning...	*/

				buffer_idx = 0;
			}
		}
	};

	extern BranchLog* log_buffer;
};
