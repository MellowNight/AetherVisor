#include "logging.h"
#include "paging_utils.h"
#include "disassembly.h"
#include "portable_executable.h"
#include "vmexit.h"
#include "utils.h"
#include "branch_tracer.h"

namespace BranchTracer
{
	bool active;
	bool initialized;

	uintptr_t start_address;

	HANDLE thread_id;

	bool is_kernel;

	void Init(VcpuData* vcpu_data, uintptr_t start_addr)
	{
		if (initialized)
		{
			return;
		}		
		
		SegmentAttribute attribute{ attribute.as_uint16 = vcpu_data->guest_vmcb.save_state_area.SsAttrib };

		is_kernel = (attribute.fields.dpl == 0) ? true : false;

		initialized = true;
		start_address = start_addr;

		int cpuinfo[4];

		__cpuid(cpuinfo, CPUID::ext_perfmon_and_debug);

		if (!(cpuinfo[0] & (1 << 1)))
		{
			DbgPrint("CPUID::ext_perfmon_and_debug::EAX %p does not support Last Branch Record Stack! \n", cpuinfo[0]);

			initialized = false;

			return;
		}

		__cpuid(cpuinfo, CPUID::svm_features);

		if (!(cpuinfo[3] & (1 << 26)))
		{
			DbgPrint("CPUID::svm_features::EDX %p does not support Last Branch Record Virtualization! \n", cpuinfo[0]);

			initialized = false;

			return;
		}
	}

	void Start(VcpuData* vcpu_data)
	{
		active = true;

		/*	BTF, trap flag, LBR stack, & LBR virtualization enable	*/

		// vcpu_data->guest_vmcb.save_state_area.DbgCtl |= (1 << IA32_DEBUGCTL_BTF_BIT);
		// vcpu_data->guest_vmcb.save_state_area.Rflags |= RFLAGS_TRAP_FLAG_BIT;
		vcpu_data->guest_vmcb.save_state_area.DBGEXTNCFG |= (1 << 6);
		vcpu_data->guest_vmcb.control_area.LbrVirtualizationEnable |= (1 << 1);

		/*	suppress recording of branches that change permission level	*/

		SegmentAttribute attribute{ attribute.as_uint16 = vcpu_data->guest_vmcb.save_state_area.SsAttrib };

		if (is_kernel)
		{
			vcpu_data->guest_vmcb.save_state_area.LBR_SELECT &= ~((uint64_t)(1 << 1));
		}
		else
		{
			vcpu_data->guest_vmcb.save_state_area.LBR_SELECT &= ~((uint64_t)(1 << 0));
		}
	}

	void BranchTracer::Stop(VcpuData* vcpu_data)
	{
		active = false;

		/*	BTF, LBR stack, and trap flag disable	*/

		vcpu_data->guest_vmcb.save_state_area.DBGEXTNCFG |= (1 << 6);
		vcpu_data->guest_vmcb.save_state_area.DbgCtl |= (1 << IA32_DEBUGCTL_BTF_BIT);
		vcpu_data->guest_vmcb.save_state_area.Rflags &= (~((uint64_t)1 << RFLAGS_TRAP_FLAG_BIT));
	}
}