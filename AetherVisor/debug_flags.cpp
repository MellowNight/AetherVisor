#include "vmexit.h"
#include "branch_tracer.h"

void VcpuData::DebugRegisterExit(GuestRegisters* guest_ctx)
{
	uint32_t gpr_num = guest_vmcb.control_area.exit_info1 & SVM_EXITINFO_REG_MASK;

	uintptr_t* target_register = NULL;

	if (gpr_num == 0)	// gpr number is rax
	{
		target_register = &guest_vmcb.save_state_area.rax;
	}
	else if (gpr_num == 4)	// gpr number is RSP
	{
		target_register = &guest_vmcb.save_state_area.rsp;
	}
	else
	{
		target_register = (*guest_ctx)[gpr_num];
	}

	if (BranchTracer::process_cr3.Flags == guest_vmcb.save_state_area.cr3.Flags)
	{
		DbgPrint("[DebugRegisterExit]	guest_rip 0x%p gpr_num %i exit_code 0x%p \n",
			guest_vmcb.save_state_area.rip, gpr_num, guest_vmcb.control_area.exit_code);

		switch (guest_vmcb.control_area.exit_code)
		{
		case VMEXIT::DR0_READ:
		{
			auto dr0 = __readdr(0);

			if (dr0 == BranchTracer::resume_address)
			{
				*target_register = NULL;
			}

			break;
		}
		case VMEXIT::DR6_READ:
		{
			*target_register = __readdr(6);

			((DR6*)target_register)->SingleInstruction = 0;

			break;
		}
		case VMEXIT::DR7_READ:
		{
			*target_register = __readdr(7);

			((DR7*)target_register)->Flags &= ~((int64_t)1 << 9);
			((DR7*)target_register)->Flags &= ~((int64_t)1 << 8);
			((DR7*)target_register)->GlobalBreakpoint0 = 0;
			((DR7*)target_register)->Length0 = 0;
			((DR7*)target_register)->ReadWrite0 = 0;

			break;
		}
		default:
		{
			break;
		}
		}

		guest_vmcb.save_state_area.rip = guest_vmcb.control_area.nrip;
	}
}

void VcpuData::PushfExit(GuestRegisters* guest_ctx)
{
	if (*(uint8_t*)guest_vmcb.save_state_area.rip == 0x66)
	{
		guest_vmcb.save_state_area.rsp -= sizeof(int16_t);

		*(int16_t*)guest_vmcb.save_state_area.rsp = (int16_t)(guest_vmcb.save_state_area.rflags.Flags & 0xFFFF);
	}
	else
	{
		guest_vmcb.save_state_area.rsp -= sizeof(uintptr_t);

		*(int64_t*)guest_vmcb.save_state_area.rsp = guest_vmcb.save_state_area.rflags.Flags;

		//((RFLAGS*)guest_vmcb.save_state_area.rsp)->ResumeFlag = 0;
		((RFLAGS*)guest_vmcb.save_state_area.rsp)->Virtual8086ModeFlag = 0;

		if (BranchTracer::process_cr3.Flags == guest_vmcb.save_state_area.cr3.Flags && BranchTracer::active)
		{
			DbgPrint("[PushfExit]	guest_rip 0x%p exit_code 0x%p \n",
				guest_vmcb.save_state_area.rip, guest_vmcb.control_area.exit_code);

			((RFLAGS*)guest_vmcb.save_state_area.rsp)->TrapFlag = 0;
		}
	}


	//DbgPrint("[PushfExit]	guest_rip 0x%p guest_vmcb.control_area.nrip 0x%p \n",
	//	guest_vmcb.save_state_area.rip, guest_vmcb.control_area.nrip);

	guest_vmcb.save_state_area.rip = guest_vmcb.control_area.nrip;
}

