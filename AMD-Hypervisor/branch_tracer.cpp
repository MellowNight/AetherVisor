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

	bool is_kernel;

	PMDL mdl;

	BranchLog* log_buffer;

	void Init(VcpuData* vcpu_data, uintptr_t start_addr, uintptr_t stop_addr, uintptr_t out_buffer, uintptr_t trace_range_base, uintptr_t trace_range_size)
	{
		auto vmroot_cr3 = __readcr3();

		__writecr3(vcpu_data->guest_vmcb.save_state_area.cr3.Flags);

		initialized = true;
		range_base = trace_range_base;
		range_size = trace_range_size;

		log_buffer = (BranchLog*)out_buffer;

		is_kernel = (vcpu_data->guest_vmcb.save_state_area.cr3.Flags == __readcr3()) ? true : false;

		DbgPrint("log_buffer  = %p \n", log_buffer);

		if (is_kernel)
		{
			mdl = PageUtils::LockPages((void*)log_buffer, IoModifyAccess, KernelMode, log_buffer->info.capacity);
		}
		else
		{
			mdl = PageUtils::LockPages((void*)log_buffer, IoModifyAccess, UserMode, log_buffer->info.capacity);
		}


		start_address = start_addr;

		stop_address = stop_addr;


		/*  place breakpoint to capture ETHREAD  */

		vcpu_data->guest_vmcb.save_state_area.Dr7.GlobalBreakpoint0 = 1;
		vcpu_data->guest_vmcb.save_state_area.Dr7.Length0 = 0;
		vcpu_data->guest_vmcb.save_state_area.Dr7.ReadWrite0 = 0;

		__writedr(0, (uintptr_t)start_addr);

		__writecr3(vmroot_cr3);
	}

	void Start(VcpuData* vcpu_data)
	{
		process_cr3 = vcpu_data->guest_vmcb.save_state_area.cr3;
		thread_id = PsGetCurrentThreadId();

		if (stop_address == 0)
		{
			stop_address = *(uintptr_t*)vcpu_data->guest_vmcb.save_state_area.Rsp;
		}

		DbgPrint("BranchTracer::stop_address  = %p \n", stop_address);

		int processor_id = KeGetCurrentProcessorNumber();

		KAFFINITY affinity = Utils::Exponent(2, processor_id);

		KeSetSystemAffinityThread(affinity);

		DbgPrint("BranchTracer::Start vcpu_data->guest_vmcb.save_state_area.Rip = %p \n", vcpu_data->guest_vmcb.save_state_area.rip);

		active = true;

		Resume(vcpu_data);
	}


	void Resume(VcpuData* vcpu_data)
	{
		if (active && PsGetCurrentThreadId() == thread_id)
		{
		//	DbgPrint("BranchTracer::Resume guest_rip = %p \n", vcpu_data->guest_vmcb.save_state_area.Rip);

			int cpuinfo[4];

			__cpuid(cpuinfo, CPUID::svm_features);

			if (!(cpuinfo[3] & (1 << 1)))
			{
				DbgPrint("CPUID::svm_features::EDX %p does not support Last Branch Record Virtualization! \n", cpuinfo[3]);

				KeBugCheckEx(MANUALLY_INITIATED_CRASH, cpuinfo[3], 0, 0, 0);
			}

			/*	BTF, LBR, and trap flag enable	*/

			vcpu_data->guest_vmcb.save_state_area.dbg_ctl.Btf = 1;
			vcpu_data->guest_vmcb.save_state_area.dbg_ctl.Lbr = 1;

			vcpu_data->guest_vmcb.save_state_area.Dr7.Flags |= (1 << 9);	// btf
			vcpu_data->guest_vmcb.save_state_area.Dr7.Flags |= (1 << 8);	// lbr

			vcpu_data->guest_vmcb.save_state_area.rflags.TrapFlag = 1;
		}
	}

	void Pause(VcpuData* vcpu_data)
	{
		if (active && PsGetCurrentThreadId() == thread_id)
		{
			DbgPrint("BranchTracer::Pause guest_rip = %p \n", vcpu_data->guest_vmcb.save_state_area.rip);

			/*	BTF, LBR, and trap flag disable	*/

			vcpu_data->guest_vmcb.save_state_area.dbg_ctl.Btf = 0;
			vcpu_data->guest_vmcb.save_state_area.dbg_ctl.Lbr = 0;

			vcpu_data->guest_vmcb.save_state_area.Dr7.Flags &= ~((int64_t)1 << 9);
			vcpu_data->guest_vmcb.save_state_area.Dr7.Flags &= ~((int64_t)1 << 8);

			vcpu_data->guest_vmcb.save_state_area.rflags.TrapFlag = 0;
		}
	}

	void Stop(VcpuData* vcpu_data)
	{
		Pause(vcpu_data);

		active = false;
	}

}