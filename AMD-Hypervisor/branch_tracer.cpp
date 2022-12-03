#include "logging.h"
#include "paging_utils.h"
#include "disassembly.h"
#include "portable_executable.h"
#include "vmexit.h"
#include "utils.h"
#include "branch_tracer.h"

void BranchTracer::Start(VcpuData* vcpu_data)
{
	active = true;

	/*	BTF, trap flag, LBR stack, & LBR virtualization enable	*/

	vcpu_data->guest_vmcb.save_state_area.DbgCtl |= (1 << IA32_DEBUGCTL_BTF_BIT);
	vcpu_data->guest_vmcb.save_state_area.Rflags |= RFLAGS_TRAP_FLAG_BIT;
	vcpu_data->guest_vmcb.save_state_area.DBGEXTNCFG |= (1 << 6);
	vcpu_data->guest_vmcb.control_area.LbrVirtualizationEnable |= (1 << 1);

	/*	suppress recording of branches that change permission level	*/

	SegmentAttribute attribute{ attribute.as_uint16 = vcpu_data->guest_vmcb.save_state_area.SsAttrib };

	auto is_kernel = (attribute.fields.dpl == 0) ? true : false;

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
	/*	BTF, LBR stack, and trap flag disable	*/

	vcpu_data->guest_vmcb.save_state_area.DBGEXTNCFG |= (1 << 6);
	vcpu_data->guest_vmcb.save_state_area.DbgCtl |= (1 << IA32_DEBUGCTL_BTF_BIT);
	vcpu_data->guest_vmcb.save_state_area.Rflags &= (~((uint64_t)1 << RFLAGS_TRAP_FLAG_BIT));
}

BranchTracer branch_tracer;