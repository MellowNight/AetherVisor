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

		*(int16_t*)save_state_area.rsp = (int16_t)(save_state_area.rflags.Flags & 0xFFFF);
	}
	else if (save_state_area.cs_attrib.fields.long_mode == 1)
	{
		last_intercept = 2;		last_rip = save_state_area.rip;


		save_state_area.rsp -= sizeof(uintptr_t);

		*(int64_t*)save_state_area.rsp = save_state_area.rflags.Flags;

		//((RFLAGS*)save_state_area.rsp)->ResumeFlag = 0;
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

		uint32_t value = save_state_area.rflags.Flags & UINT32_MAX;

		*(uint32_t*)save_state_area.rsp = value;

		//((RFLAGS*)save_state_area.rsp)->ResumeFlag = 0;
		((EFLAGS*)save_state_area.rsp)->Virtual8086ModeFlag = 0;

		if (BranchTracer::process_cr3.Flags == save_state_area.cr3.Flags && BranchTracer::active)
		{
			((RFLAGS*)save_state_area.rsp)->TrapFlag = 0;
		}
	}

	////DbgPrint("[PushfExit]	guest_rip 0x%p guest_vmcb.control_area.nrip 0x%p \n",
	//	save_state_area.rip, guest_vmcb.control_area.nrip);

	save_state_area.rip = control_area.nrip;
}

void VcpuData::PopfExit(GuestRegisters* guest_ctx)
{
	VmcbSaveStateArea& save_state_area = guest_vmcb.save_state_area;

	VmcbControlArea& control_area = guest_vmcb.control_area;

	auto operand_size = 64;

	auto iopl = save_state_area.rflags.IoPrivilegeLevel;

	if (save_state_area.cs_attrib.fields.long_mode == 0)
	{
		operand_size = 32;
	}

	if (*(uint8_t*)save_state_area.rip == 0x66)
	{
		operand_size = 16;
	}

	if (save_state_area.cpl == 0) 
	{
		uint64_t change_mask =
			RFLAGS_CARRY_FLAG_FLAG | RFLAGS_PARITY_FLAG_FLAG | RFLAGS_AUXILIARY_CARRY_FLAG_FLAG |
			RFLAGS_ZERO_FLAG_FLAG | RFLAGS_SIGN_FLAG_FLAG | RFLAGS_TRAP_FLAG_FLAG | RFLAGS_INTERRUPT_ENABLE_FLAG_FLAG |
			RFLAGS_DIRECTION_FLAG_FLAG | RFLAGS_OVERFLOW_FLAG_FLAG | RFLAGS_IO_PRIVILEGE_LEVEL_FLAG |
			RFLAGS_NESTED_TASK_FLAG_FLAG | RFLAGS_ALIGNMENT_CHECK_FLAG_FLAG | RFLAGS_IDENTIFICATION_FLAG_FLAG;

		switch (operand_size)
		{
			case 32:
			{		
				last_intercept = 4;		last_rip = save_state_area.rip;



				auto flags_val = *(uint32_t*)save_state_area.rsp;

				save_state_area.rflags.Flags = (save_state_area.rflags.Flags & (uint32_t)~change_mask) | (flags_val & (uint32_t)change_mask);
				save_state_area.rflags.ResumeFlag = 0;

				// 32-bit pop
				// All non-reserved flags except RF, VIP, VIF, and VM can be modified;
				// VIP, VIF, VM, and all reserved bits are unaffected. RF is cleared.
				break;
			}
			case 64:
			{last_intercept = 5;		last_rip = save_state_area.rip;

				auto flags_val = *(uint64_t*)save_state_area.rsp;

				save_state_area.rflags.Flags = (save_state_area.rflags.Flags & (uint64_t)~change_mask) | (flags_val & (uint64_t)change_mask);
				save_state_area.rflags.ResumeFlag = 0;

				// 64-bit pop
				// All non-reserved flags except RF, VIP, VIF, and VM can be modified;
				// VIP, VIF, VM, and all reserved bits are unaffected. RF is cleared.
				break;
			}
			case 16:
			{ // OperandSize = 16
				last_intercept = 6;		last_rip = save_state_area.rip;

				auto flags_val = *(uint16_t*)save_state_area.rsp;

				change_mask |= (RFLAGS_RESUME_FLAG_FLAG | RFLAGS_VIRTUAL_INTERRUPT_FLAG_FLAG |
					RFLAGS_VIRTUAL_8086_MODE_FLAG_FLAG | RFLAGS_VIRTUAL_INTERRUPT_PENDING_FLAG_FLAG);

				save_state_area.rflags.Flags = (save_state_area.rflags.Flags & (uint16_t)~change_mask) | (flags_val & (uint16_t)change_mask);

				// 16-bit pop
				// All non-reserved flags can be modified.
				break;
			}
			default:
			{
				__debugbreak();
				break;
			}
		}
	}
	else if (save_state_area.cpl > 0)
	{ // CPL > 0
		uint64_t change_mask =
			RFLAGS_CARRY_FLAG_FLAG | RFLAGS_PARITY_FLAG_FLAG | RFLAGS_AUXILIARY_CARRY_FLAG_FLAG |
			RFLAGS_ZERO_FLAG_FLAG | RFLAGS_SIGN_FLAG_FLAG | RFLAGS_TRAP_FLAG_FLAG |
			RFLAGS_DIRECTION_FLAG_FLAG | RFLAGS_OVERFLOW_FLAG_FLAG | RFLAGS_NESTED_TASK_FLAG_FLAG | 
			RFLAGS_ALIGNMENT_CHECK_FLAG_FLAG | RFLAGS_IDENTIFICATION_FLAG_FLAG;

		switch (operand_size)
		{
			case 32:
			{
				if (save_state_area.cpl > iopl) 
				{
					last_intercept = 7;		last_rip = save_state_area.rip;


					auto flags_val = *(uint32_t*)save_state_area.rsp;

					save_state_area.rflags.Flags = (save_state_area.rflags.Flags & (uint32_t)~change_mask) | (flags_val & (uint32_t)change_mask);
					save_state_area.rflags.ResumeFlag = 0;

					//DbgPrint("[PopfExit]	flags_val %p save_state_area.rflags %p \n", flags_val, save_state_area.rflags.Flags);

					// 32-bit pop
					// All non-reserved bits except IF, IOPL, VIP, VIF, VM and RF can be modified;
					// IF, IOPL, VIP, VIF, VM and all reserved bits are unaffected; RF is cleared.
				}
				else
				{
					last_rip = save_state_area.rip;

					last_intercept = 8;

					change_mask |= RFLAGS_INTERRUPT_ENABLE_FLAG_FLAG;

					auto flags_val = *(uint32_t*)save_state_area.rsp;

					save_state_area.rflags.Flags = (save_state_area.rflags.Flags & (uint32_t)~change_mask) | (flags_val & (uint32_t)change_mask);
					save_state_area.rflags.ResumeFlag = 0;
					
					// 32-bit pop
					// All non-reserved bits except IOPL, VIP, VIF, VM and RF can be modified;
					// IOPL, VIP, VIF, VM and all reserved bits are unaffected; RF is cleared.
				}

				if (BranchTracer::process_cr3.Flags == save_state_area.cr3.Flags && BranchTracer::active)
				{
					save_state_area.rflags.TrapFlag = 1;
				}

				break;
			}
			case 64:
			{
				if (save_state_area.cpl > iopl)
				{
					last_intercept = 9;		last_rip = save_state_area.rip;


					auto flags_val = *(uint64_t*)save_state_area.rsp;

					save_state_area.rflags.Flags = (save_state_area.rflags.Flags & (uint64_t)~change_mask) | (flags_val & (uint64_t)change_mask);
					save_state_area.rflags.ResumeFlag = 0;
					
					// 64-bit pop
					// All non-reserved bits except IF, IOPL, VIP, VIF, VM and RF can be modified;
					// IF, IOPL, VIP, VIF, VM and all reserved bits are unaffected; RF is cleared.
				}
				else
				{
					last_intercept = 10;		last_rip = save_state_area.rip;


					change_mask |= RFLAGS_INTERRUPT_ENABLE_FLAG_FLAG;

					auto flags_val = *(uint64_t*)save_state_area.rsp;

					save_state_area.rflags.Flags = (save_state_area.rflags.Flags & (uint64_t)~change_mask) | (flags_val & (uint64_t)change_mask);
					save_state_area.rflags.ResumeFlag = 0;
					
					// 64-bit pop
					// All non-reserved bits except IOPL, VIP, VIF, VM and RF can be modified;
					// IOPL, VIP, VIF, VM and all reserved bits are unaffected; RF is cleared.
				}

				if (BranchTracer::process_cr3.Flags == save_state_area.cr3.Flags && BranchTracer::active)
				{
					save_state_area.rflags.TrapFlag = 1;
				}

				break;
			}
			case 16:
			{ // OperandSize = 16
				last_intercept = 11;		last_rip = save_state_area.rip;


				change_mask |= (RFLAGS_RESUME_FLAG_FLAG | RFLAGS_INTERRUPT_ENABLE_FLAG_FLAG | RFLAGS_VIRTUAL_INTERRUPT_FLAG_FLAG |
					RFLAGS_VIRTUAL_8086_MODE_FLAG_FLAG | RFLAGS_VIRTUAL_INTERRUPT_PENDING_FLAG_FLAG);

				auto flags_val = *(uint16_t*)save_state_area.rsp;

				save_state_area.rflags.Flags = (save_state_area.rflags.Flags & (uint16_t)~change_mask) | (flags_val & (uint16_t)change_mask);

				// 16-bit pop
				// All non-reserved bits except IOPL can be modified; IOPL and all
				// reserved bits are unaffected.
				break;
			}
			default:
			{
				__debugbreak();
				break;
			}
		}
	}

	save_state_area.rsp += (operand_size / 8);

	save_state_area.rip = control_area.nrip;
}

