#include "logging.h"
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
	uintptr_t captured_rip;

	uint32_t tls_index;

	HANDLE thread_id;

	CR3 process_cr3;

	int is_system;

	void Init(VcpuData* vcpu, uintptr_t start_addr, uintptr_t stop_addr, uintptr_t trace_range_base, uintptr_t trace_range_size, uint32_t tls_idx)
	{
		initialized = true;
		range_base = trace_range_base;
		range_size = trace_range_size;
		process_cr3 = vcpu->guest_vmcb.save_state_area.cr3;
		tls_index = tls_idx;
		is_system = ((uintptr_t)start_addr < 0x7FFFFFFFFFFF) ? false : true;
		start_address = start_addr;
		stop_address = stop_addr;

		DbgPrint("[BranchTracer::Init]	tls_idx: %p \n", tls_idx);
	}

	void Start(VcpuData* vcpu)
	{
		thread_id = PsGetCurrentThreadId();

		if (stop_address == NULL)
		{
			stop_address = *(uintptr_t*)vcpu->guest_vmcb.save_state_area.rsp;
		}

		DbgPrint("BranchTracer::stop_address  = %p \n", stop_address);

		int processor_id = KeGetCurrentProcessorNumber();

		KAFFINITY affinity = Utils::Exponent(2, processor_id);

		KeSetSystemAffinityThread(affinity);

		DbgPrint("BranchTracer::Start vcpu->guest_vmcb.save_state_area.Rip = %p \n\n", vcpu->guest_vmcb.save_state_area.rip);

		active = true;

		NptHooks::ForEachHook(
			[](auto hook_entry, auto data) -> auto {

				if (hook_entry->address == data)
				{
					DbgPrint("[BreakpointHandler]   hook_entry->address == data, found stealth breakpoint! \n");

					UnsetHook(hook_entry);
				}
				return false;
			},
			(void*)vcpu->guest_vmcb.save_state_area.rip
		);

		/*  clean TLB after removing the NPT hook   */

		vcpu->guest_vmcb.control_area.vmcb_clean &= 0xFFFFFFEF;
		vcpu->guest_vmcb.control_area.tlb_control = 1;


		Resume(vcpu);
	}


	void Resume(VcpuData* vcpu)
	{
		if (active && PsGetCurrentThreadId() == thread_id)
		{
			DbgPrint("[BranchTracer::Resume]	guest_rip = %p \n\n", vcpu->guest_vmcb.save_state_area.rip);

			int cpuinfo[4];

			__cpuid(cpuinfo, CPUID::svm_features);

			if (!(cpuinfo[3] & (1 << 1)))
			{
				DbgPrint("CPUID::svm_features::EDX %p does not support Last Branch Record Virtualization! \n", cpuinfo[3]);

				KeBugCheckEx(MANUALLY_INITIATED_CRASH, cpuinfo[3], 0, 0, 0);
			}

			vcpu->guest_vmcb.control_area.lbr_virt_enable |= (1 << 0);

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
			DbgPrint("[BranchTracer::Pause]	guest_rip = %p \n\n", vcpu->guest_vmcb.save_state_area.rip);

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

	void UpdateState(VcpuData* vcpu)
	{
		auto guest_vmcb = vcpu->guest_vmcb;

		auto guest_rip = guest_vmcb.save_state_area.rip;

		if ((guest_vmcb.save_state_area.dr7.Flags & ((uint64_t)1 << 9)) &&
			(guest_vmcb.save_state_area.cr3.Flags == BranchTracer::process_cr3.Flags))
		{
			if (guest_rip < BranchTracer::range_base || guest_rip >(BranchTracer::range_size + BranchTracer::range_base))
			{
				/*  Pause branch tracer after a branch outside of the specified range is executed.
					Single-stepping mode => completely disabled
				*/

				BranchTracer::Pause(vcpu);

				return;
			}

			DbgPrint("[UpdateState]		LastBranchFromIP %p guest_rip = %p  \n\n\n", guest_vmcb.save_state_area.br_from, guest_rip);

			/*  Do not trace the branch hook itself */

			auto tls_ptr = Utils::GetTlsPtr(guest_vmcb.save_state_area.gs_base, tls_index);

			if (!*tls_ptr)
			{
				*tls_ptr = TRUE;

				captured_rip = guest_rip;

				Instrumentation::InvokeHook(
					vcpu, Instrumentation::branch, &guest_vmcb.save_state_area.br_from, sizeof(uintptr_t));

				return;
			}
			else if (captured_rip == guest_rip)
			{
				DbgPrint("[UpdateState]		Branch hook finished \n");

				captured_rip = NULL;
				*tls_ptr = FALSE;
			}

			if (guest_rip == BranchTracer::stop_address)
			{
				BranchTracer::Stop(vcpu);

				Instrumentation::InvokeHook(vcpu, Instrumentation::branch_trace_finished);
			}

			return;
		}
	}
}