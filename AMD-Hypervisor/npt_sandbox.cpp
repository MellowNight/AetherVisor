#include "npt_sandbox.h"
#include "logging.h"
#include "paging_utils.h"
#include "disassembly.h"
#include "portable_executable.h"
#include "vmexit.h"
#include "utils.h"

namespace Sandbox
{
	void* sandbox_hooks[2] = { NULL, NULL };

	int sandbox_page_count;
	SandboxPage* sandbox_page_array;

	void Init()
	{
		/*	reserve memory for hooks because we can't allocate memory in VM root	*/

		int max_hooks = 6000;

		sandbox_page_array = (SandboxPage*)ExAllocatePoolZero(NonPagedPool, sizeof(SandboxPage) * max_hooks, 'hook');

		for (int i = 0; i < max_hooks; ++i)
		{
			sandbox_page_array[i].Init();
		}

		sandbox_page_count = 0;
	}

	void InstructionInstrumentation(VcpuData* vcpu_data, uintptr_t guest_rip, GeneralRegisters* guest_regs, bool is_kernel)
	{
		auto vmroot_cr3 = __readcr3();

		if (!is_kernel)
		{
			__writecr3(vcpu_data->guest_vmcb.save_state_area.Cr3);
		}

		ZydisDecodedOperand operands[5] = { 0 };

		ZydisRegisterContext context;

		Disasm::MyRegContextToZydisRegContext(vcpu_data, guest_regs, &context);

		auto instruction = Disasm::Disassemble((uint8_t*)guest_rip, operands);

		if (instruction.meta.category == ZydisInstructionCategory::ZYDIS_CATEGORY_COND_BR ||
			instruction.meta.category == ZydisInstructionCategory::ZYDIS_CATEGORY_UNCOND_BR ||
			instruction.meta.category == ZydisInstructionCategory::ZYDIS_CATEGORY_CALL)
		{
			/*	handle calls/jmps (execute_target is wrong here)	*/

			//auto execute_target = Disasm::GetMemoryAccessTarget(instruction, operands, (uintptr_t)guest_rip, &context);

			//DbgPrint("\n execute_target %p \n", execute_target);
			DbgPrint("guest_rip %p \n", guest_rip);

			if (!is_kernel)
			{
				if (guest_rip && (guest_rip < 0x7FFFFFFFFFFF))
				{
					vcpu_data->guest_vmcb.save_state_area.Rip = (uintptr_t)Sandbox::sandbox_hooks[execute_handler];

					vcpu_data->guest_vmcb.save_state_area.Rsp -= 8;
					*(uintptr_t*)vcpu_data->guest_vmcb.save_state_area.Rsp = guest_rip;
				}
				else
				{
					DbgPrint("ADDRESS SPACE MISMATCH \n");
				}
			}
			else
			{
				if (guest_rip && (guest_rip > 0x7FFFFFFFFFFF))
				{
					vcpu_data->guest_vmcb.save_state_area.Rip = (uintptr_t)Sandbox::sandbox_hooks[execute_handler];

					vcpu_data->guest_vmcb.save_state_area.Rsp -= 8;
					*(uintptr_t*)vcpu_data->guest_vmcb.save_state_area.Rsp = guest_rip;
				}
				else
				{
					DbgPrint("ADDRESS SPACE MISMATCH \n");
				}
			}

			vcpu_data->guest_vmcb.control_area.NCr3 = Hypervisor::Get()->ncr3_dirs[primary];
		}

		if (!is_kernel)
		{
			__writecr3(vmroot_cr3);
		}

		return;
	}

	SandboxPage* ForEachHook(bool(HookCallback)(SandboxPage* hook_entry, void* data), void* callback_data)
	{
		for (int i = 0; i < sandbox_page_count; ++i)
		{
			if (HookCallback(&sandbox_page_array[i], callback_data))
			{
				return &sandbox_page_array[i];
			}
		}
		return 0;
	}

	void ReleasePage(SandboxPage* hook_entry)
	{
		hook_entry->hookless_npte->ExecuteDisable = 0;
		hook_entry->active = false;

		PageUtils::UnlockPages(hook_entry->mdl);

		sandbox_page_count -= 1;
	}

	void DenyMemoryAccess(VcpuData* vmcb_data, void* address)
	{
		/*	set nPTE->present = 0	*/

		auto vmroot_cr3 = __readcr3();

		__writecr3(vmcb_data->guest_vmcb.save_state_area.Cr3);

		auto physical_page = PAGE_ALIGN(MmGetPhysicalAddress(address).QuadPart);

		auto sandbox_npte = PageUtils::GetPte(physical_page, Hypervisor::Get()->ncr3_dirs[sandbox]);

		sandbox_npte->Present = 0;

		/*	DenyMemoryAccess epilogue	*/

		__writecr3(vmroot_cr3);

		vmcb_data->guest_vmcb.control_area.TlbControl = 3;
	}

	/*
		IMPORTANT: if you want to set a hook in a globally mapped DLL such as ntdll.dll, you must trigger copy on write first!	
		Sandbox::AddPageToSandbox() is basically just a modified version of NPTHooks::SetNptHook
	*/

	SandboxPage* AddPageToSandbox(VcpuData* vmcb_data, void* address, int32_t tag)
	{
		/*	enable execute for the nPTE of the guest address in the sandbox NCR3	*/

		auto vmroot_cr3 = __readcr3();

		__writecr3(vmcb_data->guest_vmcb.save_state_area.Cr3);

		auto sandbox_entry = &sandbox_page_array[sandbox_page_count];

		if ((uintptr_t)address < 0x7FFFFFFFFFF)
		{
			sandbox_entry->mdl = PageUtils::LockPages(address, IoReadAccess, UserMode);
		}
		else
		{
			sandbox_entry->mdl = PageUtils::LockPages(address, IoReadAccess, KernelMode);
		}

		auto physical_page = PAGE_ALIGN(MmGetPhysicalAddress(address).QuadPart);

		DbgPrint("AddPageToSandbox() physical_page = %p \n", physical_page);

		sandbox_page_count += 1;

		sandbox_entry->active = true;
		sandbox_entry->tag = tag;

		/*	disable execute on the nested pte of the guest physical address, in NCR3 1	*/

		sandbox_entry->hookless_npte = PageUtils::GetPte(physical_page, Hypervisor::Get()->ncr3_dirs[primary]);

		sandbox_entry->hookless_npte->ExecuteDisable = 1;

		auto sandbox_npte = PageUtils::GetPte(physical_page, Hypervisor::Get()->ncr3_dirs[sandbox]);

		sandbox_npte->ExecuteDisable = 0;

		/*	IsolatePage epilogue	*/

		__writecr3(vmroot_cr3);

		vmcb_data->guest_vmcb.control_area.TlbControl = 3;

		return sandbox_entry;
	}

};
