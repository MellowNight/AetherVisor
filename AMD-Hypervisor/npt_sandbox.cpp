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
		hook_entry->active = false;

		PageUtils::UnlockPages(hook_entry->mdl);

		sandbox_page_count -= 1;
	}

	void DenyMemoryAccess(VcpuData* vmcb_data, void* address)
	{
		/*	set nPTE->present = 0	*/

		auto vmroot_cr3 = __readcr3();

		__writecr3(vmcb_data->guest_vmcb.save_state_area.Cr3.Flags);

		auto sandbox_entry = &sandbox_page_array[sandbox_page_count];

		if ((uintptr_t)address < 0x7FFFFFFFFFF)
		{
			sandbox_entry->mdl = PageUtils::LockPages(address, IoReadAccess, UserMode);
		}
		else
		{
			sandbox_entry->mdl = PageUtils::LockPages(address, IoReadAccess, KernelMode);
		}

		sandbox_page_count += 1;

		sandbox_entry->unreadable = true;

		sandbox_entry->guest_physical = PAGE_ALIGN(MmGetPhysicalAddress(address).QuadPart);

		auto sandbox_npte = PageUtils::GetPte(sandbox_entry->guest_physical, Hypervisor::Get()->ncr3_dirs[sandbox]);

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

		__writecr3(vmcb_data->guest_vmcb.save_state_area.Cr3.Flags);

		auto sandbox_entry = &sandbox_page_array[sandbox_page_count];

		if ((uintptr_t)address < 0x7FFFFFFFFFF)
		{
			sandbox_entry->mdl = PageUtils::LockPages(address, IoReadAccess, UserMode);
		}
		else
		{
			sandbox_entry->mdl = PageUtils::LockPages(address, IoReadAccess, KernelMode);
		}

		sandbox_entry->guest_physical = PAGE_ALIGN(MmGetPhysicalAddress(address).QuadPart);

		DbgPrint("AddPageToSandbox() physical_page = %p \n", sandbox_entry->guest_physical);

		sandbox_page_count += 1;

		sandbox_entry->active = true;
		sandbox_entry->tag = tag;

		/*	disable execute on the nested pte of the guest physical address, in NCR3 1	*/

		sandbox_entry->primary_npte = PageUtils::GetPte(sandbox_entry->guest_physical, Hypervisor::Get()->ncr3_dirs[primary]);

		sandbox_entry->primary_npte->ExecuteDisable = 1;

		auto sandbox_npte = PageUtils::GetPte(sandbox_entry->guest_physical, Hypervisor::Get()->ncr3_dirs[sandbox]);

		sandbox_npte->ExecuteDisable = 0;

		/*	IsolatePage epilogue	*/

		__writecr3(vmroot_cr3);

		vmcb_data->guest_vmcb.control_area.TlbControl = 3;

		return sandbox_entry;
	}

};
