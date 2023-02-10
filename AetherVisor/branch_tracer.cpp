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
	uintptr_t stop_address;
	uintptr_t range_base;
	uintptr_t range_size;

	HANDLE thread_id;
	CR3 process_cr3;

	bool is_system;

	PMDL mdl;

	BranchLog* log_buffer;

	void Init(VcpuData* vcpu, uintptr_t start_addr, uintptr_t stop_addr, uintptr_t out_buffer, uintptr_t trace_range_base, uintptr_t trace_range_size)
	{
		initialized = true;
		range_base = trace_range_base;
		range_size = trace_range_size;

		log_buffer = (BranchLog*)out_buffer;

		is_system = (vcpu->guest_vmcb.save_state_area.cr3.Flags == __readcr3()) ? true : false;

		// DbgPrint("log_buffer  = %p \n", log_buffer);

		if (is_system)
		{
			mdl = Utils::LockPages((void*)log_buffer, IoModifyAccess, KernelMode, log_buffer->info.capacity);
		}
		else
		{
			mdl = Utils::LockPages((void*)log_buffer, IoModifyAccess, UserMode, log_buffer->info.capacity);
		}

		start_address = start_addr;

		stop_address = stop_addr;


		/*  place breakpoint to capture ETHREAD  */

		vcpu->guest_vmcb.save_state_area.dr7.GlobalBreakpoint0 = 1;
		vcpu->guest_vmcb.save_state_area.dr7.Length0 = 0;
		vcpu->guest_vmcb.save_state_area.dr7.ReadWrite0 = 0;

		__writedr(0, (uintptr_t)start_addr);
	}

	void Start(VcpuData* vcpu)
	{
		process_cr3 = vcpu->guest_vmcb.save_state_area.cr3;
		thread_id = PsGetCurrentThreadId();

		if (stop_address == NULL)
		{
			stop_address = *(uintptr_t*)vcpu->guest_vmcb.save_state_area.rsp;
		}

		// DbgPrint("BranchTracer::stop_address  = %p \n", stop_address);

		int processor_id = KeGetCurrentProcessorNumber();

		KAFFINITY affinity = Utils::Exponent(2, processor_id);

		KeSetSystemAffinityThread(affinity);

		// DbgPrint("BranchTracer::Start vcpu->guest_vmcb.save_state_area.Rip = %p \n", vcpu->guest_vmcb.save_state_area.rip);

		active = true;

		Resume(vcpu);
	}


	void Resume(VcpuData* vcpu)
	{
		if (active && PsGetCurrentThreadId() == thread_id)
		{
		//	DbgPrint("BranchTracer::Resume guest_rip = %p \n", vcpu->guest_vmcb.save_state_area.Rip);

			int cpuinfo[4];

			__cpuid(cpuinfo, CPUID::svm_features);

			if (!(cpuinfo[3] & (1 << 1)))
			{
				DbgPrint("CPUID::svm_features::EDX %p does not support Last Branch Record Virtualization! \n", cpuinfo[3]);

				KeBugCheckEx(MANUALLY_INITIATED_CRASH, cpuinfo[3], 0, 0, 0);
			}

			/*	BTF, LBR, and trap flag enable	*/

			vcpu->guest_vmcb.save_state_area.dbg_ctl.Btf = 1;
			vcpu->guest_vmcb.save_state_area.dbg_ctl.Lbr = 1;

			vcpu->guest_vmcb.save_state_area.dr7.Flags |= (1 << 9);	// btf
			vcpu->guest_vmcb.save_state_area.dr7.Flags |= (1 << 8);	// lbr

			vcpu->guest_vmcb.save_state_area.rflags.TrapFlag = 1;
		}
	}

	void Pause(VcpuData* vcpu)
	{
		if (active && PsGetCurrentThreadId() == thread_id)
		{
			DbgPrint("BranchTracer::Pause guest_rip = %p \n", vcpu->guest_vmcb.save_state_area.rip);

			/*	BTF, LBR, and trap flag disable	*/

			vcpu->guest_vmcb.save_state_area.dbg_ctl.Btf = 0;
			vcpu->guest_vmcb.save_state_area.dbg_ctl.Lbr = 0;

			vcpu->guest_vmcb.save_state_area.dr7.Flags &= ~((int64_t)1 << 9);
			vcpu->guest_vmcb.save_state_area.dr7.Flags &= ~((int64_t)1 << 8);

			vcpu->guest_vmcb.save_state_area.rflags.TrapFlag = 0;
		}
	}

	void Stop(VcpuData* vcpu)
	{
		Pause(vcpu);

		active = false;
	}

}