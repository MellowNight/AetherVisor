#include "npt_hook.h"
#include "logging.h"
#include "paging_utils.h"
#include "disassembly.h"
#include "portable_executable.h"
#include "vmexit.h"
#include "utils.h"

namespace NptHooks
{
	int hook_count;
	NptHook* npt_hook_array;

	void Init()
	{
		/*	reserve memory for hooks because we can't allocate memory in VM root	*/

		int max_hooks = 500;
		
		npt_hook_array = (NptHook*)ExAllocatePoolZero(NonPagedPool, sizeof(NptHook) * max_hooks, 'hook');

		for (int i = 0; i < max_hooks; ++i)
		{
			npt_hook_array[i].Init();
		}

		hook_count = 0;
	}

	NptHook* ForEachHook(bool(HookCallback)(NptHook* hook_entry, void* data), void* callback_data)
	{
		for (int i = 0; i < hook_count; ++i)
		{
			if (HookCallback(&npt_hook_array[i], callback_data))
			{
				return &npt_hook_array[i];
			}
		}
		return 0;
	}

	void UnsetHook(NptHook* hook_entry)
	{
		hook_entry->hookless_npte->ExecuteDisable = 0;
		hook_entry->process_cr3 = 0;
		hook_entry->active = false;
		hook_entry->guest_pte->ExecuteDisable = hook_entry->original_nx;

		PageUtils::UnlockPages(hook_entry->mdl);

		hook_count -= 1;
	}


	/*	IMPORTANT: if you want to set a hook in a globally mapped DLL such as ntdll.dll, you must trigger copy on write first!	*/

	NptHook* SetNptHook(CoreData* vmcb_data, void* address, uint8_t* patch, size_t patch_len, int32_t noexecute_cr3_id, int32_t tag)
	{
		auto vmroot_cr3 = __readcr3();

		__writecr3(vmcb_data->guest_vmcb.save_state_area.Cr3);

		bool reused_hook = false;

		auto hook_entry = &npt_hook_array[hook_count];

		if ((uintptr_t)address < 0x7FFFFFFFFFF)
		{
			hook_entry->mdl = PageUtils::LockPages(address, IoReadAccess, UserMode);
		}
		else
		{
			hook_entry->mdl = PageUtils::LockPages(address, IoReadAccess, KernelMode);
		}

		auto physical_page = PAGE_ALIGN(MmGetPhysicalAddress(address).QuadPart);

		hook_count += 1;

		hook_entry->active = true;
		hook_entry->tag = tag;
		hook_entry->process_cr3 = vmcb_data->guest_vmcb.save_state_area.Cr3;
		hook_entry->noexecute_ncr3 = Hypervisor::Get()->ncr3_dirs[noexecute_cr3_id];

		/*	get the guest pte and physical address of the hooked page	*/

		hook_entry->guest_physical_page = (uint8_t*)physical_page;

		hook_entry->guest_pte = PageUtils::GetPte((void*)address, hook_entry->process_cr3);

		hook_entry->original_nx = hook_entry->guest_pte->ExecuteDisable;

		hook_entry->guest_pte->ExecuteDisable = 0;
		hook_entry->guest_pte->Write = 1;


		/*	get the nested pte of the guest physical address	*/

		hook_entry->hookless_npte = PageUtils::GetPte((void*)physical_page, Hypervisor::Get()->ncr3_dirs[primary]);

		if (hook_entry->hookless_npte->ExecuteDisable == 1)
		{
			/*	this page was already hooked	*/

			reused_hook = true;
		}

		hook_entry->hookless_npte->ExecuteDisable = 1;


		/*	get the nested pte of the guest physical address in the 2nd NCR3, and map it to our hook page	*/

		auto hooked_npte = PageUtils::GetPte((void*)physical_page, Hypervisor::Get()->ncr3_dirs[noexecute_cr3_id]);

		hooked_npte->PageFrameNumber = hook_entry->hooked_pte->PageFrameNumber;
		hooked_npte->ExecuteDisable = 0;
		
		auto hooked_copy = PageUtils::VirtualAddrFromPfn(hooked_npte->PageFrameNumber);

		auto page_offset = (uintptr_t)address & (PAGE_SIZE - 1);


		/*	place our hook in the copied page for the 2nd NCR3, and don't overwrite already existing hooks on the page	*/

		if (!reused_hook)
		{
			memcpy(hooked_copy, PAGE_ALIGN(address), PAGE_SIZE);
		}

		memcpy((uint8_t*)hooked_copy + page_offset, patch, patch_len);

		/*	SetNptHook epilogue	*/

		vmcb_data->guest_vmcb.control_area.TlbControl = 3;

		__writecr3(vmroot_cr3);

		return hook_entry;
	}

};
