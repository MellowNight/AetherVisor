#include "npt_hook.h"
#include "logging.h"
#include "paging_utils.h"
#include "disassembly.h"
#include "portable_executable.h"
#include "vmexit.h"
#include "utils.h"

namespace NptHooks
{
	void Init()
	{
		/*	reserve memory for hooks because we can't allocate memory in VM root	*/

		int max_hooks = 500;
		
		InitializeListHead((PLIST_ENTRY)&first_npt_hook);

		first_npt_hook.Init();

		for (int i = 0; i < max_hooks; ++i)
		{
			auto hook_entry = (NptHook*)ExAllocatePool(NonPagedPool, sizeof(NptHook));

			hook_entry->Init();

			InsertTailList((PLIST_ENTRY)&first_npt_hook, (PLIST_ENTRY)hook_entry);
		}

		hook_count = 1;
	}

	NptHook* ForEachHook(bool(HookCallback)(NptHook* hook_entry, void* data), void* callback_data)
	{
		auto hook_entry = &first_npt_hook;

		for (int i = 0; i < hook_count; hook_entry = (NptHook*)hook_entry->list_entry.Flink, ++i)
		{
			if (HookCallback(hook_entry, callback_data))
			{
				return hook_entry;
			}
		}
		return 0;
	}

	void UnsetHook(NptHook* hook_entry)
	{
		hook_entry->hookless_npte->ExecuteDisable = 0;
		hook_entry->process_cr3 = 0;
		hook_entry->active = false;

		hook_count -= 1;
	}


	/*	IMPORTANT: if you want to set a hook in a globally mapped DLL such as ntdll.dll, you must trigger copy on write first!	*/

	NptHook* SetNptHook(CoreData* vmcb_data, void* address, uint8_t* patch, size_t patch_len, int32_t tag)
	{
		auto vmroot_cr3 = __readcr3();

		__writecr3(vmcb_data->guest_vmcb.save_state_area.Cr3);

		auto physical_page = PAGE_ALIGN(MmGetPhysicalAddress(address).QuadPart);

		auto hook_entry = &first_npt_hook;

		bool reused_hook = false;

		for (int i = 0; i < hook_count, hook_entry->active; hook_entry = (NptHook*)hook_entry->list_entry.Flink, ++i)
		{
			if (physical_page == hook_entry->guest_physical_page)
			{
				reused_hook = true;
				break;
			}
		}		

		hook_count += 1;

		hook_entry->active = true;
		hook_entry->tag = tag;
		hook_entry->process_cr3 = vmcb_data->guest_vmcb.save_state_area.Cr3;

		/*	get the guest pte and physical address of the hooked page	*/

		hook_entry->guest_physical_page = (uint8_t*)physical_page;

		auto guest_pte = PageUtils::GetPte((void*)address, hook_entry->process_cr3);

		guest_pte->ExecuteDisable = 0;

		/*	get the nested pte of the guest physical address	*/

		hook_entry->hookless_npte = PageUtils::GetPte((void*)physical_page, Hypervisor::Get()->normal_ncr3);
		hook_entry->hookless_npte->ExecuteDisable = 1;


		/*	get the nested pte of the guest physical address in the 2nd NCR3, and map it to our hook page	*/

		auto hooked_npte = PageUtils::GetPte((void*)physical_page, Hypervisor::Get()->noexecute_ncr3);

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
