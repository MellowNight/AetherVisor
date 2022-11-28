#include "logging.h"
#include "paging_utils.h"
#include "disassembly.h"
#include "portable_executable.h"
#include "vmexit.h"
#include "utils.h"

namespace BranchTracer
{
	int traced_path_count;
    
	CtlFlowTrace* trace_list;

	void Init()
	{
		/*	reserve memory for hooks because we can't allocate memory in VM root	*/

		int max_traces = 5;

		control_flow_list = (ControlFlow*)ExAllocatePoolZero(NonPagedPool, sizeof(ControlFlow) * max_hooks, 'hook');

		for (int i = 0; i < max_hooks; ++i)
		{
			trace_list[i].Init();
		}

		traced_path_count = 0;
	}

	CtlFlowTrace* ForEachTrace(bool(HookCallback)(ControlFlow* hook_entry, void* data), void* callback_data)
	{
		for (int i = 0; i < traced_path_count; ++i)
		{
			if (HookCallback(&trace_list[i], callback_data))
			{
				return &trace_list[i];
			}
		}
		return 0;
	}

	void ReleasePage(ControlFlow* hook_entry)
	{
		hook_entry->hookless_npte->ExecuteDisable = 0;
		hook_entry->active = false;

		PageUtils::UnlockPages(hook_entry->mdl);

		sandbox_page_count -= 1;
	}

    /*  
        StartTrace(): Add an address to start tracing branches from
    */

	CtlFlowTrace* StartTrace(VcpuData* vmcb_data, void* address, int32_t tag)
	{
		/*	enable execute for the nPTE of the guest address in the sandbox NCR3	*/

		auto vmroot_cr3 = __readcr3();

		__writecr3(vmcb_data->guest_vmcb.save_state_area.Cr3);

		auto control_flow = &sandbox_page_array[sandbox_page_count];

		if ((uintptr_t)address < 0x7FFFFFFFFFF)
		{
			control_flow->mdl = PageUtils::LockPages(address, IoReadAccess, UserMode);
		}
		else
		{
			control_flow->mdl = PageUtils::LockPages(address, IoReadAccess, KernelMode);
		}

		auto physical_page = PAGE_ALIGN(MmGetPhysicalAddress(address).QuadPart);

		DbgPrint("AddPageToSandbox() physical_page = %p \n", physical_page);

		traced_path_count += 1;

		control_flow->active = true;
		control_flow->tag = tag;

        control_flow->start_address = address;

		/*	IsolatePage epilogue	*/

		__writecr3(vmroot_cr3);

		vmcb_data->guest_vmcb.control_area.TlbControl = 3;

		return control_flow;
	}

};
