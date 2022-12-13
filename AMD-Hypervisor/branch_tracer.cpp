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

	NPTHooks::NptHook* capture_thread_bp;

	void Init(VcpuData* vcpu_data, uintptr_t start_addr, uintptr_t out_buffer)
	{
		auto vmroot_cr3 = __readcr3();

		__writecr3(vcpu_data->guest_vmcb.save_state_area.Cr3);

		initialized = true;

		SegmentAttribute attribute{ attribute.as_uint16 = vcpu_data->guest_vmcb.save_state_area.SsAttrib };

		is_kernel = (attribute.fields.dpl == 0) ? true : false;

		log_buffer = (BranchLog*)out_buffer;

		DbgPrint("log_buffer  = %p \n", log_buffer);

		if (is_kernel)
		{
			mdl = PageUtils::LockPages((void*)log_buffer, IoReadAccess, KernelMode, log_buffer->capacity);
		}
		else
		{
			mdl = PageUtils::LockPages((void*)log_buffer, IoReadAccess, UserMode, log_buffer->capacity);
		}
		DbgPrint("log_buffer  = %p \n", log_buffer);

		start_address = start_addr;

		/*  place breakpoint to capture ETHREAD  */

		DR7 dr7;
		dr7.Flags = vcpu_data->guest_vmcb.save_state_area.Dr7;

		dr7.GlobalBreakpoint0 = 1;
		dr7.Length0 = 0;
		dr7.ReadWrite0 = 0;

		vcpu_data->guest_vmcb.save_state_area.Dr7 = dr7.Flags;

		__writedr(0, (uintptr_t)start_addr);

		__writecr3(vmroot_cr3);
	}

	void Start(VcpuData* vcpu_data)
	{
		int cpuinfo[4];

		__cpuid(cpuinfo, CPUID::svm_features);

		if (!(cpuinfo[3] & (1 << 1)))
		{
			DbgPrint("CPUID::svm_features::EDX %p does not support Last Branch Record Virtualization! \n", cpuinfo[3]);

			KeBugCheckEx(MANUALLY_INITIATED_CRASH, cpuinfo[3], 0, 0, 0);
		}

		active = true;

		DbgPrint("1 vcpu_data->guest_vmcb.save_state_area.DbgCtl %p \n", vcpu_data->guest_vmcb.save_state_area.DbgCtl);

		/*	BTF, trap flag, LBR stack enable	*/

		vcpu_data->guest_vmcb.save_state_area.DbgCtl |= (1 << IA32_DEBUGCTL_BTF_BIT);
		vcpu_data->guest_vmcb.save_state_area.Rflags |= (1 << RFLAGS_TRAP_FLAG_BIT);

		DbgPrint("2 vcpu_data->guest_vmcb.save_state_area.DbgCtl %p \n", vcpu_data->guest_vmcb.save_state_area.DbgCtl);

		//	vcpu_data->guest_vmcb.save_state_area.DBGEXTNCFG |= (1 << 6);
	}

	void Start()
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