#include "logging.h"
#include "paging_utils.h"
#include "disassembly.h"
#include "portable_executable.h"
#include "vmexit.h"
#include "utils.h"

namespace BranchTracer
{
    struct CtlFlowTrace
    {
        uintptr_t tag;
        bool active;
        uint8_t* start_address;
        uint8_t* last_branch_address;
    };

	extern int traced_path_count;
	extern ControlFlow* control_flow_list;

	void Init();

	ControlFlow* ForEachTrace(bool(HookCallback)(ControlFlow* hook_entry, void* data), void* callback_data);

	void ReleasePage(ControlFlow* hook_entry);

	ControlFlow* AddCode(VcpuData* vmcb_data, void* address, int32_t tag);
};
