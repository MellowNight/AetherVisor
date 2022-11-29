#include "logging.h"
#include "paging_utils.h"
#include "disassembly.h"
#include "portable_executable.h"
#include "vmexit.h"
#include "utils.h"

namespace BranchTracer
{
	extern bool active;
	extern int lbr_stack_size;

	void Init();

	ControlFlow* ForEachTrace(bool(HookCallback)(ControlFlow* hook_entry, void* data), void* callback_data);

	void ReleasePage(ControlFlow* hook_entry);

	ControlFlow* AddCode(VcpuData* vmcb_data, void* address, int32_t tag);
};
