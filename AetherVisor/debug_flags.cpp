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

	/*	DR7 debug register access detection	*/

	if (guest_vmcb.save_state_area.dr7.GeneralDetect)
	{
		DR6 dr6; dr6.Flags = __readdr(6);
		
		dr6.BreakpointCondition = 0;
		dr6.DebugRegisterAccessDetected = 1;

		__writedr(6, dr6.Flags);

		guest_vmcb.save_state_area.dr7.GeneralDetect = FALSE;

		InjectException(EXCEPTION_VECTOR::Debug, FALSE, 0);

		suppress_nrip_increment = TRUE;
		return;
	}

	//DbgPrint("[DebugRegisterExit]	guest_rip 0x%p gpr_num %i exit_code 0x%p \n",
		//guest_vmcb.save_state_area.rip, gpr_num, guest_vmcb.control_area.exit_code);

	switch (guest_vmcb.control_area.exit_code)
	{
	case VMEXIT::DR0_READ:
	{
		auto dr0 = __readdr(0);

		if (/*BranchTracer::process_cr3.Flags == guest_vmcb.save_state_area.cr3.Flags && */BranchTracer::active)
		{
			*target_register = NULL;
		}

		break;
	}
	case VMEXIT::DR6_READ:
	{
		*target_register = __readdr(6);

		if (/*BranchTracer::process_cr3.Flags == guest_vmcb.save_state_area.cr3.Flags &&*/ BranchTracer::active)
		{
			((DR6*)target_register)->SingleInstruction = 0;
		}
		break;
	}
	case VMEXIT::DR7_READ:
	{
		*target_register = __readdr(7);

		if (/*BranchTracer::process_cr3.Flags == guest_vmcb.save_state_area.cr3.Flags && */BranchTracer::active)
		{
			((DR7*)target_register)->Flags &= ~((int64_t)1 << 9);
			((DR7*)target_register)->Flags &= ~((int64_t)1 << 8);
			((DR7*)target_register)->GlobalBreakpoint0 = 0;
			((DR7*)target_register)->Length0 = 0;
			((DR7*)target_register)->ReadWrite0 = 0;
		}
		break;
	}
	default:
	{
		break;
	}
	}	
}

int last_intercept = 0;
uintptr_t last_rip = 0;

void VcpuData::PushfExit(GuestRegisters* guest_ctx)
{
	VmcbSaveStateArea& save_state_area = guest_vmcb.save_state_area;
	VmcbControlArea& control_area = guest_vmcb.control_area;

	if (*(uint8_t*)save_state_area.rip == 0x66)
	{
		last_intercept = 1;
		last_rip = save_state_area.rip;

		save_state_area.rsp -= sizeof(int16_t);

		*(int16_t*)save_state_area.rsp = (int16_t)((save_state_area.rflags.Flags & 0xFFFF000000000000LL) >> 48);
	}
	else if (save_state_area.cs_attrib.fields.long_mode == 1)
	{
		last_intercept = 2;		last_rip = save_state_area.rip;


		save_state_area.rsp -= sizeof(uintptr_t);

		*(int64_t*)save_state_area.rsp = save_state_area.rflags.Flags;

		((RFLAGS*)save_state_area.rsp)->ResumeFlag = 0;
		((RFLAGS*)save_state_area.rsp)->Virtual8086ModeFlag = 0;

		if (BranchTracer::process_cr3.Flags == save_state_area.cr3.Flags && BranchTracer::active)
		{
			((RFLAGS*)save_state_area.rsp)->TrapFlag = 0;
		}
	}
	else if (save_state_area.cs_attrib.fields.long_mode == 0)
	{
		last_intercept = 3;		last_rip = save_state_area.rip;


		save_state_area.rsp -= sizeof(uint32_t);

		uint32_t value = ((save_state_area.rflags.Flags & 0xFFFFFFFF00000000LL) >> 32);

		*(uint32_t*)save_state_area.rsp = value;

		((RFLAGS*)save_state_area.rsp)->ResumeFlag = 0;
		((EFLAGS*)save_state_area.rsp)->Virtual8086ModeFlag = 0;

		if (BranchTracer::process_cr3.Flags == save_state_area.cr3.Flags && BranchTracer::active)
		{
			((RFLAGS*)save_state_area.rsp)->TrapFlag = 0;
		}
	}

	////DbgPrint("[PushfExit]	guest_rip 0x%p guest_vmcb.control_area.nrip 0x%p \n",
	//	save_state_area.rip, guest_vmcb.control_area.nrip);
}

void VcpuData::PopfExit(GuestRegisters* guest_ctx)
{
	VmcbSaveStateArea& save_state_area = guest_vmcb.save_state_area;

	VmcbControlArea& control_area = guest_vmcb.control_area;

	if (!IsPagePresent((void*)save_state_area.rsp))
	{
		return;
	}

	uint32_t unchanged_mask = RFLAGS_VIRTUAL_INTERRUPT_PENDING_FLAG_FLAG | RFLAGS_VIRTUAL_INTERRUPT_FLAG_FLAG | RFLAGS_VIRTUAL_8086_MODE_FLAG_FLAG;

    if ( save_state_area.cpl > 0 )
	{
        unchanged_mask |= RFLAGS_IO_PRIVILEGE_LEVEL_FLAG;

		if (save_state_area.cpl > save_state_area.rflags.IoPrivilegeLevel)
		{
			unchanged_mask |= RFLAGS_INTERRUPT_ENABLE_FLAG_FLAG;
		}
    }

	RFLAGS stack_flags { stack_flags.Flags = 0 };

	uint32_t operand_size = sizeof(uint32_t);;

	stack_flags.Flags = *(uint32_t*)save_state_area.rsp;

    /* 64-bit mode: POP defaults to a 64-bit operand. */

	if (save_state_area.cs_attrib.fields.long_mode)
	{
		stack_flags.Flags = *(uint64_t*)save_state_area.rsp;

		operand_size = sizeof(uint64_t);
	}

	if (*(uint8_t*)save_state_area.rip == 0x66)
	{
		stack_flags.Flags = *(uint16_t*)save_state_area.rsp;

		operand_size = sizeof(uint16_t);

		stack_flags.Flags = (uint16_t)stack_flags.Flags | (save_state_area.rflags.Flags & 0xffff0000u);

	}
	
	stack_flags.Flags &= 0x257fd5;

	save_state_area.rflags.Flags &= unchanged_mask;
	save_state_area.rflags.Flags |= (uint32_t)(stack_flags.Flags & ~unchanged_mask) | 0x02;

	if (BranchTracer::active && BranchTracer::process_cr3.Flags == save_state_area.cr3.Flags)
	{
		save_state_area.rflags.TrapFlag = 1;
	}


	save_state_area.rsp += operand_size;
}

