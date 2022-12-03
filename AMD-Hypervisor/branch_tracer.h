#include "logging.h"
#include "paging_utils.h"
#include "disassembly.h"
#include "portable_executable.h"
#include "vmexit.h"
#include "utils.h"

class BranchTracer
{
public:

	BranchTracer() {}

	BranchTracer(uintptr_t start_addr, void* output_buf, int out_buf_size)
		: start_address(start_addr), log_buffer((LogBuffer*)output_buf), log_buf_size(out_buf_size)
	{
		mdl = PageUtils::LockPages(log_buffer, IoWriteAccess, UserMode, out_buf_size);

		int cpuinfo[4];

		__cpuid(cpuinfo, CPUID::ext_perfmon_and_debug);

		if (!(cpuinfo[0] & (1 << 1)))
		{
			DbgPrint("CPUID::ext_perfmon_and_debug::EAX %p does not support Last Branch Record Stack! \n", cpuinfo[0]);
			return;
		}

		__cpuid(cpuinfo, CPUID::svm_features);

		if (!(cpuinfo[3] & (1 << 26)))
		{
			DbgPrint("CPUID::svm_features::EDX %p does not support Last Branch Record Virtualization! \n", cpuinfo[0]);
			return;
		}
	}

	bool active;	
	bool initialized;

	struct BasicBlock
	{
		uintptr_t start;
		uintptr_t end;
	};

	struct LogBuffer
	{
		BasicBlock*	cur_block;
		BasicBlock	records[1];
	};

	uintptr_t start_address;
	uintptr_t last_branch;

	MDL* mdl;

	int log_buf_size;

	LogBuffer* log_buffer;

	void Start(VcpuData* vcpu_data);
	void Stop(VcpuData* vcpu_data);
};

extern BranchTracer branch_tracer;