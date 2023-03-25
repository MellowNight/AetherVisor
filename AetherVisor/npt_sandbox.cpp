#include "npt_sandbox.h"
#include "logging.h"
#include "disassembly.h"
#include "portable_executable.h"
#include "vmexit.h"
#include "utils.h"

namespace Sandbox
{
	int sandbox_page_count;

	SandboxPage* sandbox_page_array;

	int max_hooks = 10000;
	
	uintptr_t branch_exclusion_range_base;

	void Init()
	{
		/*	reserve memory for hooks because we can't allocate memory in VM root	*/

		sandbox_page_array = (SandboxPage*)ExAllocatePoolZero(NonPagedPool, sizeof(SandboxPage) * max_hooks, 'hook');

		sandbox_page_count = 0;
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
		// Utils::UnlockPages(hook_entry->mdl);

		if (hook_entry->primary_npte)
		{
			hook_entry->primary_npte->ExecuteDisable = 0;
		}		
		
		hook_entry->denied_access = FALSE;
		hook_entry->guest_physical = NULL;

		if ((hook_entry->id != sandbox_page_count - 1) && hook_entry->id != 0)
		{
			/*	shift every entry back 1 index	*/

			memmove(&sandbox_page_array[hook_entry->id - 1], &sandbox_page_array[hook_entry->id], sizeof(SandboxPage) * (sandbox_page_count - hook_entry->id));
		}

		sandbox_page_count -= 1;
	}

	/*	Trap on memory accesses to a certain page from within the sandbox. 
		- allow_reads 1: read only
		- allow_reads 0: non-present nPTE, no access
	*/

	void DenyMemoryAccess(VcpuData* vcpu, void* address, bool allow_reads)
	{
		if (!vcpu->IsPagePresent(address))
		{
			return;
		}
		else
		{
			/*	attach to the guest process context	*/

			auto sandbox_entry = &sandbox_page_array[sandbox_page_count];

			KPROCESSOR_MODE mode = (vcpu->guest_vmcb.save_state_area.cpl == 3) ? UserMode : KernelMode;

			sandbox_entry->process_cr3 = __readcr3();

			sandbox_entry->id = sandbox_page_count;

			sandbox_entry->mdl = Utils::LockPages(address, IoReadAccess, mode);

			sandbox_entry->denied_access = !allow_reads;

			sandbox_entry->guest_physical = PAGE_ALIGN(MmGetPhysicalAddress(address).QuadPart);

			auto sandbox_npte = Utils::GetPte(sandbox_entry->guest_physical, Hypervisor::Get()->ncr3_dirs[sandbox]);

			sandbox_page_count += 1;

			if (allow_reads)
			{
				/*	read only, only trap on writes	*/

				sandbox_npte->Present = 1;
				sandbox_npte->Write = 0;
			}
			else
			{
				/*	no access, trap on both reads and writes	*/

				sandbox_npte->Present = 0;
				sandbox_npte->Write = 0;
			}

			vcpu->guest_vmcb.control_area.tlb_control = 3;
		}
	}


	/*
		IMPORTANT: if you want to set a hook in a globally mapped DLL such as ntdll.dll, you must trigger copy on write first!	
		
		Sandbox::AddPageToSandbox() is basically just a modified version of NPTHooks::SetNptHook
	*/

	SandboxPage* AddPageToSandbox(VcpuData* vmcb_data, void* address, int32_t tag)
	{
		if (!vmcb_data->IsPagePresent(address))
		{
			return NULL;
		}
		else
		{
			/*	enable execute for the nPTE of the guest address in the sandbox NCR3	*/

			auto sandbox_entry = &sandbox_page_array[sandbox_page_count];

			KPROCESSOR_MODE mode = (vmcb_data->guest_vmcb.save_state_area.cpl == 3) ? UserMode : KernelMode;

			sandbox_entry->id = sandbox_page_count;

			sandbox_entry->process_cr3 = __readcr3();

	//		sandbox_entry->mdl = Utils::LockPages(address, IoReadAccess, mode);

			sandbox_entry->guest_physical = PAGE_ALIGN(MmGetPhysicalAddress(address).QuadPart);

			// Logger::Get()->LogJunk("AddPageToSandbox() physical_page = %p address = %p \n", sandbox_entry->guest_physical, address);

			sandbox_page_count += 1;

			/*	disable execute on the nested pte of the guest physical address, in NCR3 1	*/

			sandbox_entry->primary_npte = Utils::GetPte(sandbox_entry->guest_physical, Hypervisor::Get()->ncr3_dirs[primary]);

			sandbox_entry->primary_npte->ExecuteDisable = 1;

			auto sandbox_npte = Utils::GetPte(sandbox_entry->guest_physical, Hypervisor::Get()->ncr3_dirs[sandbox]);

			sandbox_npte->ExecuteDisable = 0;

			/*	IsolatePage epilogue	*/

			vmcb_data->guest_vmcb.control_area.tlb_control = 3;

			return sandbox_entry;
		}
	}

};
