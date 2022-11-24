#include "npt_sandbox.h"
#include "logging.h"
#include "paging_utils.h"
#include "disassembly.h"
#include "portable_executable.h"
#include "vmexit.h"
#include "utils.h"

namespace Sandbox
{
	void* sandbox_handler = NULL;
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

	uint8_t* EmulateInstruction(VcpuData* vcpu_data, uint8_t* guest_rip, GeneralRegisters* guest_regs)
	{
		uint8_t* execute_target = 0;

		ZydisDecodedOperand operands[5];

		ZydisRegisterContext context;

		Disasm::HvRegContextToZydisRegContext(vcpu_data, guest_regs, &context);

		auto instruction = Disasm::Disassemble(guest_rip, operands);

		if (instruction.meta.category == ZydisInstructionCategory::ZYDIS_CATEGORY_COND_BR ||
			instruction.meta.category == ZydisInstructionCategory::ZYDIS_CATEGORY_UNCOND_BR ||
			instruction.meta.category == ZydisInstructionCategory::ZYDIS_CATEGORY_CALL)
		{
			execute_target = (uint8_t*)Disasm::GetCallJmpTarget(instruction, operands, (uintptr_t)guest_rip, &context);
		}
		else
		{

		}

		return execute_target;
	}

	void LogSandboxPageAccess(VcpuData* vcpu_data, GeneralRegisters* guest_context, PHYSICAL_ADDRESS faulting_physical)
	{
		auto guest_rip = vcpu_data->guest_vmcb.save_state_area.Rip;

		/*	if the RIP is inside of a sandbox page	*/

		//auto sandboxed_page = Sandbox::ForEachHook(
		//	[](SandboxPage* sandboxed_page, void* data)-> bool {

		//		if (sandboxed_page->guest_virtual_page == data)
		//		{
		//			return true;
		//		}

		//		return false;
		//	},
		//	(void*)PAGE_ALIGN(guest_rip)
		//);

		BOOL is_system_page = (__readcr3() == vcpu_data->guest_vmcb.save_state_area.Cr3) ? TRUE : FALSE;

		if (is_system_page)
		{	
			auto target_guest_rip = MmGetVirtualForPhysical(faulting_physical);

			if (((uintptr_t)target_guest_rip > 0x7FFFFFFFFFFF))
			{
				vcpu_data->guest_vmcb.save_state_area.Rip = (uintptr_t)sandbox_handler;
				vcpu_data->guest_vmcb.save_state_area.Rsp -= 8;

				*(uintptr_t*)vcpu_data->guest_vmcb.save_state_area.Rsp = (uintptr_t)target_guest_rip;
			}
		}
		else
		{
			auto vmroot_cr3 = __readcr3();

			__writecr3(vcpu_data->guest_vmcb.save_state_area.Cr3);

			auto execute_target = (uintptr_t)EmulateInstruction(vcpu_data, (uint8_t*)guest_rip);

			if (execute_target && (execute_target < 0x7FFFFFFFFFFF))
			{
				vcpu_data->guest_vmcb.save_state_area.Rip = (uintptr_t)sandbox_handler;

				vcpu_data->guest_vmcb.save_state_area.Rsp -= 8;
				*(uintptr_t*)vcpu_data->guest_vmcb.save_state_area.Rsp = (uintptr_t)execute_target;

			}
			__writecr3(vmroot_cr3);
		}
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
		hook_entry->process_cr3 = 0;
		hook_entry->active = false;
		hook_entry->guest_pte->ExecuteDisable = hook_entry->original_nx;

		PageUtils::UnlockPages(hook_entry->mdl);

		sandbox_page_count -= 1;
	}

	/*	
	
		IMPORTANT: if you want to set a hook in a globally mapped DLL such as ntdll.dll, you must trigger copy on write first!	
		Sandbox::IsolatePage() is basically just a modified version of NPTHooks::SetNptHook
	*/

	SandboxPage* IsolatePage(VcpuData* vmcb_data, void* address, int32_t tag)
	{
		auto vmroot_cr3 = __readcr3();

		__writecr3(vmcb_data->guest_vmcb.save_state_area.Cr3);

		auto hook_entry = &sandbox_page_array[sandbox_page_count];

		if ((uintptr_t)address < 0x7FFFFFFFFFF)
		{
			hook_entry->mdl = PageUtils::LockPages(address, IoReadAccess, UserMode);
		}
		else
		{
			hook_entry->mdl = PageUtils::LockPages(address, IoReadAccess, KernelMode);
		}

		auto physical_page = PAGE_ALIGN(MmGetPhysicalAddress(address).QuadPart);

		sandbox_page_count += 1;

		hook_entry->active = true;
		hook_entry->tag = tag;
		hook_entry->process_cr3 = vmcb_data->guest_vmcb.save_state_area.Cr3;

		/*	get the guest pte and physical address of the sandboxed page	*/

		hook_entry->guest_virtual_page = (uint8_t*)address;
		hook_entry->guest_physical_page = (uint8_t*)physical_page;

		hook_entry->guest_pte = PageUtils::GetPte((void*)address, hook_entry->process_cr3);

		hook_entry->original_nx = hook_entry->guest_pte->ExecuteDisable;

		hook_entry->guest_pte->ExecuteDisable = 0;
		hook_entry->guest_pte->Write = 1;


		/*	disable execute on the nested pte of the guest physical address, in NCR3 1	*/

		hook_entry->hookless_npte = PageUtils::GetPte((void*)physical_page, Hypervisor::Get()->ncr3_dirs[primary]);

		hook_entry->hookless_npte->ExecuteDisable = 1;


		/*	assign a new nested pte for the guest physical address in the sandbox NCR3	*/

		PHYSICAL_ADDRESS sandbox_ncr3;

		sandbox_ncr3.QuadPart = Hypervisor::Get()->ncr3_dirs[NCR3_DIRECTORIES::sandbox];

		auto pml4_base = (PML4E_64*)MmGetVirtualForPhysical(sandbox_ncr3);

		auto hooked_npte = AssignNPTEntry(pml4_base, (uintptr_t)physical_page, PTEAccess{ true, true, true });

		auto hooked_copy = PageUtils::VirtualAddrFromPfn(hooked_npte->PageFrameNumber);

		memcpy((uint8_t*)hooked_copy, PAGE_ALIGN(address), PAGE_SIZE);

		/*	SetNptHook epilogue	*/

		vmcb_data->guest_vmcb.control_area.TlbControl = 3;

		__writecr3(vmroot_cr3);

		return hook_entry;
	}

};
