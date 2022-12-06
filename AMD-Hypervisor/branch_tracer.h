#include "logging.h"
#include "paging_utils.h"
#include "disassembly.h"
#include "portable_executable.h"
#include "vmexit.h"
#include "utils.h"

namespace BranchTracer
{
	void Init(VcpuData* vcpu_data, uintptr_t start_addr);

	void Start(VcpuData* vcpu_data);
	void Stop(VcpuData* vcpu_data);

	extern bool active;	
	extern bool initialized;

	extern uintptr_t start_address;

	extern HANDLE thread_id;
};
