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

	PMDL mdl;

	BranchLog* log_buffer;

	void Init(VcpuData* vcpu_data, uintptr_t start_addr, uintptr_t out_buffer)
	{
		initialized = true;

		SegmentAttribute attribute{ attribute.as_uint16 = vcpu_data->guest_vmcb.save_state_area.SsAttrib };

		is_kernel = (attribute.fields.dpl == 0) ? true : false;

		log_buffer = (BranchLog*)out_buffer;

		if (is_kernel)
		{
			mdl = PageUtils::LockPages((void*)log_buffer, IoReadAccess, KernelMode, log_buffer->capacity);
		}
		else
		{
			mdl = PageUtils::LockPages((void*)log_buffer, IoReadAccess, UserMode, log_buffer->capacity);
		}

		start_address = start_addr;
	}

	void Resume(VcpuData* vcpu_data)
	{
		active = true;

		/*	BTF, trap flag, LBR stack, & LBR virtualization enable	*/

		vcpu_data->guest_vmcb.save_state_area.DbgCtl |= (1 << IA32_DEBUGCTL_BTF_BIT);
		vcpu_data->guest_vmcb.save_state_area.Rflags |= RFLAGS_TRAP_FLAG_BIT;
	//	vcpu_data->guest_vmcb.save_state_area.DBGEXTNCFG |= (1 << 6);
	//	vcpu_data->guest_vmcb.control_area.LbrVirtualizationEnable |= (1 << 1);
	}

	void Resume()
	{
		/*	BTF and trap flag set	*/

		active = true;

		IA32_DEBUGCTL_REGISTER debugctl;

		debugctl.Flags = __readmsr(IA32_DEBUGCTL);
		debugctl.Btf = 1;

		__writemsr(IA32_DEBUGCTL, debugctl.Flags);

		RFLAGS flags;

		flags.Flags = __readeflags();
		flags.TrapFlag = 1;

		__writeeflags(flags.Flags);
	}

	void BranchTracer::Stop(VcpuData* vcpu_data)
	{
		active = false;

		/*	BTF, LBR stack, and trap flag disable	*/

	//	vcpu_data->guest_vmcb.save_state_area.DBGEXTNCFG |= (1 << 6);
		vcpu_data->guest_vmcb.save_state_area.DbgCtl &= (~((uint64_t)1 << IA32_DEBUGCTL_BTF_BIT));
		vcpu_data->guest_vmcb.save_state_area.Rflags &= (~((uint64_t)1 << RFLAGS_TRAP_FLAG_BIT));
	}
}