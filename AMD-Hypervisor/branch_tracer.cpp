#include "logging.h"
#include "paging_utils.h"
#include "disassembly.h"
#include "portable_executable.h"
#include "vmexit.h"
#include "utils.h"

namespace BranchTracer
{
	bool active;
	int lbr_stack_size;
    
	void StartTrace(VcpuData* vcpu_data)
	{
		active = true;

		int cpuinfo[4];

		__cpuidex(&cpuinfo, CPUID::ext_perfmon_and_debug)

		if (!(cpuinfo[0] & (1 << 1)))
		{
			DbgPrint("CPUID::ext_perfmon_and_debug::EAX %p does not support Last Branch Record Stack! \n", cpuinfo[0]);
			return;
		}

		__cpuidex(&cpuinfo, CPUID::svm_features)

		if (!(cpuinfo[3] & (1 << 26)))
		{
			DbgPrint("CPUID::svm_features::EDX %p does not support Last Branch Record Virtualization! \n", cpuinfo[0]);
			return;
		}

		/*	LBR stack & LBR virtualization enable	*/

		vcpu_data->guest_vmcb.control_area.LbrVirtualizationEnable |= (1 << 1);

		vcpu_data->guest_vmcb.save_state_area.DBGEXTNCFG |= (1 << 6);

		/*	suppress recording of branches that change permission level	*/

		SEGMENT_ATTRIBUTE attribute{ attribute.AsUInt16 = VpData->GuestVmcb.StateSaveArea.SsAttrib };

		auto is_kernel = (attribute.Fields.Dpl == DPL_SYSTEM) ? true : false;

		if (is_kernel)
        {
			vcpu_data->guest_vmcb.save_state_area.LBR_SELECT &= ~((uint64_t)(1 << 1));   
        }
		else 
		{
			vcpu_data->guest_vmcb.save_state_area.LBR_SELECT &= ~((uint64_t)(1 << 0));
		}
	}
};
