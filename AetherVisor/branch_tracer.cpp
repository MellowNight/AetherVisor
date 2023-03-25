#include "logging.h"
#include "disassembly.h"
#include "portable_executable.h"
#include "vmexit.h"
#include "utils.h"
#include "branch_tracer.h"

using namespace Instrumentation;

namespace BranchTracer
{
	bool lbr_active; // only determines whether or not the LBR flag is set
	bool active;
	bool initialized;

	uintptr_t start_address;
	uintptr_t stop_address;

	uintptr_t range_base;
	uintptr_t range_size;

	uintptr_t resume_address;

	HANDLE thread_id;

	CR3 process_cr3;

	int is_system;

	void Init(VcpuData* vcpu, uintptr_t start_addr, uintptr_t stop_addr, uintptr_t trace_range_base, uintptr_t trace_range_size)
	{
		initialized = true;
		range_base = trace_range_base;
		range_size = trace_range_size;
		process_cr3 = vcpu->guest_vmcb.save_state_area.cr3;
		is_system = ((uintptr_t)start_addr < 0x7FFFFFFFFFFF) ? false : true;
		start_address = start_addr;
		stop_address = stop_addr;
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

		//	DbgPrint("BranchTracer::Start vcpu->guest_vmcb.save_state_area.Rip = %p \n\n", vcpu->guest_vmcb.save_state_area.rip);

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

	void SetLBR(VcpuData* vcpu, BOOL value)
	{			
		lbr_active = value ? true : false;

		if (value)
		{
			vcpu->guest_vmcb.control_area.lbr_virt_enable |= (1 << 0);

			vcpu->guest_vmcb.save_state_area.dbg_ctl.Lbr = 1;
			vcpu->guest_vmcb.save_state_area.dr7.Flags |= (1 << 8);	// lbr
		}
		else
		{
			vcpu->guest_vmcb.save_state_area.dbg_ctl.Lbr = 0;
			vcpu->guest_vmcb.save_state_area.dr7.Flags &= ~((int64_t)1 << 8);
		}
	}

	void SetBTF(VcpuData* vcpu, BOOL value)
	{
		if (value)
		{
			vcpu->guest_vmcb.save_state_area.dbg_ctl.Btf = 1;
			vcpu->guest_vmcb.save_state_area.dr7.Flags |= (1 << 9);	// btf
		}
		else
		{
			vcpu->guest_vmcb.save_state_area.dbg_ctl.Btf = 0;
			vcpu->guest_vmcb.save_state_area.dr7.Flags &= ~((int64_t)1 << 9);
		}
	}

	void Resume(VcpuData* vcpu)
	{
		if (active && PsGetCurrentThreadId() == thread_id)
		{
			//	DbgPrint("[BranchTracer::Resume]	guest_rip = %p \n\n", vcpu->guest_vmcb.save_state_area.rip);

			int cpuinfo[4];

			__cpuid(cpuinfo, CPUID::svm_features);

			if (!(cpuinfo[3] & (1 << 1)))
			{
				DbgPrint("CPUID::svm_features::EDX %p does not support Last Branch Record Virtualization! \n", cpuinfo[3]);

				KeBugCheckEx(MANUALLY_INITIATED_CRASH, cpuinfo[3], 0, 0, 0);
			}

			SetBTF(vcpu, TRUE);
			SetLBR(vcpu, TRUE);

			/*	BTF, LBR, and trap flag enable	*/

			vcpu->guest_vmcb.save_state_area.rflags.TrapFlag = 1;
		}
	}

	void Pause(VcpuData* vcpu)
	{
		if (active && PsGetCurrentThreadId() == thread_id)
		{
			//	DbgPrint("[BranchTracer::Pause]	guest_rip = %p \n\n", vcpu->guest_vmcb.save_state_area.rip);

				/*	BTF, LBR, and trap flag disable	*/

			SetBTF(vcpu, FALSE);
			SetLBR(vcpu, FALSE);

			vcpu->guest_vmcb.save_state_area.rflags.TrapFlag = 0;
		}
	}

	void Stop(VcpuData* vcpu)
	{
		DbgPrint("[BranchTracer::Stop]		BRANCH TRACING FINISHED!!!!!!!!!!! rip %p \n", vcpu->guest_vmcb.save_state_area.rip);

		Pause(vcpu);

		active = false;
	}

	void UpdateState(VcpuData* vcpu, GuestRegisters* guest_ctx)
	{
		auto guest_vmcb = vcpu->guest_vmcb;

		auto guest_rip = guest_vmcb.save_state_area.rip;

		if ((guest_vmcb.save_state_area.dr7.Flags & ((uint64_t)1 << 9)) &&
			(guest_vmcb.save_state_area.cr3.Flags == BranchTracer::process_cr3.Flags))
		{

			//			DbgPrint("[UpdateState]		LastBranchFromIP %p guest_rip = %p  \n\n\n", guest_vmcb.save_state_area.br_from, guest_rip);

						/*	completely stop the branch tracer	*/

			if (guest_rip == BranchTracer::stop_address)
			{
				BranchTracer::Stop(vcpu);

				Instrumentation::InvokeHook(vcpu, Instrumentation::branch_trace_finished);

				return;
			}

			/*
				Do not trace the branch hook itself.

				Pause branch tracer when a branch to outside of the specified range occurs,
				or upon invoking branch callback.

				Single-stepping mode => completely disabled
			*/

			auto tls_buffer = Utils::GetTlsPtr<TlsParams>(guest_vmcb.save_state_area.gs_base, callbacks[branch].tls_params_idx);

			if ((*tls_buffer)->callback_pending == false)
			{
				if (!Instrumentation::InvokeHook(vcpu, Instrumentation::branch))
				{
					DbgPrint("[UpdateState]	stack read fucked up, retry the #DB, guest rip %p \n", guest_rip);

					vcpu->InjectException(EXCEPTION_VECTOR::Debug, FALSE, 0);

					return;
				}

				(*tls_buffer)->last_branch_from = (void*)guest_vmcb.save_state_area.br_from;
				(*tls_buffer)->callback_pending = true;

				if (guest_rip < BranchTracer::range_base || guest_rip >(BranchTracer::range_size + BranchTracer::range_base))
				{
					resume_address = *(uintptr_t*)guest_vmcb.save_state_area.rsp;
				}
				else
				{
					resume_address = guest_rip;
				}

				vcpu->guest_vmcb.save_state_area.dr7.GlobalBreakpoint0 = 1;
				vcpu->guest_vmcb.save_state_area.dr7.Length0 = 0;
				vcpu->guest_vmcb.save_state_area.dr7.ReadWrite0 = 0;

				__writedr(0, (uintptr_t)resume_address);

				BranchTracer::Pause(vcpu);

				return;
			}
			else
			{
				// Branch single-step is currently being handled if TLS variable is TRUE
			}

			return;
		}
	}
}