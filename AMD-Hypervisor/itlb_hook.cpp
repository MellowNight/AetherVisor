#include "itlb_hook.h"

namespace TlbHooker
{
	int hook_count;
	SplitTlbHook* first_tlb_hook;

	SplitTlbHook* SetTlbHook(uintptr_t address, char* patch, size_t patch_len)
	{
		auto hook_entry = first_tlb_hook;

		for (int i = 0; i < hook_count; hook_entry = hook_entry->next_hook, ++i)
		{
		}

		auto hookless_pfn = hook_entry->hookless_pte.PageFrameNumber;

		auto copy_page = Utils::VirtualAddrFromPfn(hookless_pfn);

		memcpy(copy_page, (uint8_t*)address, PAGE_SIZE);

		return hook_entry;
	}


	SplitTlbHook* FindByHooklessPhysicalPage(uint64_t page_physical)
	{
		PFN_NUMBER pfn = page_physical >> PAGE_SHIFT;

		for (auto hook_entry = first_tlb_hook; hook_entry->next_hook != NULL;
			hook_entry = hook_entry->next_hook)
		{
			if (hook_entry->hookless_pte.PageFrameNumber == pfn)
			{
				return hook_entry;
			}
		}

		return 0;
	}

	void Init()
	{
		auto first_hookless_page = (uintptr_t)ExAllocatePool(NonPagedPool, PAGE_SIZE);
		
		first_tlb_hook = (SplitTlbHook*)ExAllocatePool(NonPagedPool, sizeof(SplitTlbHook));
		first_tlb_hook->hookless_pte.PageFrameNumber = Utils::PfnFromVirtualAddr(first_hookless_page);

		auto hook_entry = first_tlb_hook;

		/*	reserve memory for hooks because we can't allocate memory in VM root	*/

		int max_hooks = 20;

		for (int i = 0; i < max_hooks; hook_entry = hook_entry->next_hook, ++i)
		{
			auto hookless_page = (uintptr_t)ExAllocatePool(NonPagedPool, PAGE_SIZE);

			hook_entry->next_hook = (SplitTlbHook*)ExAllocatePool(NonPagedPool, sizeof(SplitTlbHook));

			hook_entry->next_hook->hookless_pte.PageFrameNumber = Utils::PfnFromVirtualAddr(hookless_page);
		}

		hook_count = 0;
	}

	void HandlePageFaultTlb(CoreVmcbData* vcpu, GPRegs* guest_regs)
	{
		auto fault_address = (void*)vcpu->guest_vmcb.control_area.ExitInfo2;

		PageFaultErrorCode fault_info;
		fault_info.as_uint32 = vcpu->guest_vmcb.control_area.ExitInfo1;

		auto hook_entry = FindByHooklessPhysicalPage(MmGetPhysicalAddress(fault_address).QuadPart);

		CR3 guest_cr3 = { vcpu->guest_vmcb.save_state_area.Cr3 };

		if (fault_info.fields.execute)
		{
			/*	Mark the page as present	*/

			auto pte = Utils::GetPte(fault_address, guest_cr3.AddressOfPageDirectory << PAGE_SHIFT,
				[](PT_ENTRY_64* pte) -> int {
					pte->Present = 1;
					return 0;
				}
			);

			hook_entry->ret_pointer();	/*	Add the iTLB entry	*/

			pte->Present = 0;	/*	Mark page as non-present, so that we can intercept data access after dTLB miss	*/
		}
		else if (fault_info.fields.valid == 0)
		{

		}
	}
};
