#include "itlb_hook.h"
#include "vmexit.h"

namespace TlbHooker
{
	int hook_count;
	SplitTlbHook first_tlb_hook;

	SplitTlbHook* SetTlbHook(void* address, uint8_t* patch, size_t patch_len)
	{
		__invlpg(address);

		auto hook_entry = &first_tlb_hook;

		for (int i = 0; i < hook_count; hook_entry = hook_entry->next_hook, ++i)
		{
		}

		CR3 cr3;
		cr3.Flags = __readcr3();

		hook_entry->hooked_pte = Utils::GetPte(address, cr3.AddressOfPageDirectory << PAGE_SHIFT);
		hook_entry->hooked_pte->ExecuteDisable = 0;
		hook_entry->hooked_page_va = PAGE_ALIGN(address);

		auto hookless_pfn = hook_entry->hookless_pte->PageFrameNumber;

		auto copy_page = Utils::VirtualAddrFromPfn(hookless_pfn);

		memcpy(copy_page, (uint8_t*)PAGE_ALIGN(address), PAGE_SIZE);

		auto irql = Utils::DisableWP();
		auto retptr = Utils::FindPattern((uintptr_t)PAGE_ALIGN(address), PAGE_SIZE, "\xCC\xCC", 2, 0x00);
		
		memcpy(address, patch, patch_len);

		/*	Find a 0xC3 in the page, for iTLB filling	*/
		*(char*)retptr = 0xC3;
		Utils::EnableWP(irql);

		hook_entry->ret_pointer = (void(*)())retptr;
		hook_entry->hooked_pte->Present = 0;

		__invlpg(address);
		hook_count += 1;

		return hook_entry;
	}


	SplitTlbHook* FindByPage(void* virtual_addr)
	{
		auto hook_entry = &first_tlb_hook;

		for (int i = 0; i < hook_count; hook_entry = hook_entry->next_hook, ++i)
		{
			if (hook_entry->hooked_page_va == virtual_addr)
			{
				return hook_entry;
			}
		}
		return 0;
	}


	SplitTlbHook* FindByHookedPhysicalPage(uint64_t page_physical)
	{
		PFN_NUMBER pfn = page_physical >> PAGE_SHIFT;
		auto hook_entry = &first_tlb_hook;

		for (int i = 0; i < hook_count; hook_entry = hook_entry->next_hook, ++i)
		{
			if (hook_entry->hooked_pte->PageFrameNumber == pfn)
			{
				return hook_entry;
			}
		}
		return 0;
	}

	void Init()
	{
		auto first_hookless_page = ExAllocatePool(NonPagedPool, PAGE_SIZE);

		CR3 cr3;
		cr3.Flags = __readcr3();

		first_tlb_hook.hookless_pte = Utils::GetPte(first_hookless_page, cr3.AddressOfPageDirectory << PAGE_SHIFT);
		first_tlb_hook.next_hook = (SplitTlbHook*)ExAllocatePool(NonPagedPool, sizeof(SplitTlbHook));

		auto hook_entry = &first_tlb_hook;

		/*	reserve memory for hooks because we can't allocate memory in VM root	*/

		int max_hooks = 20;

		for (int i = 0; i < max_hooks; hook_entry = hook_entry->next_hook, ++i)
		{
			auto hookless_page = (uintptr_t)ExAllocatePool(NonPagedPool, PAGE_SIZE);

			hook_entry->next_hook = (SplitTlbHook*)ExAllocatePool(NonPagedPool, sizeof(SplitTlbHook));

			hook_entry->next_hook->hookless_pte = Utils::GetPte(first_hookless_page, cr3.AddressOfPageDirectory << PAGE_SHIFT);
		}

		hook_count = 0;
	}

	void HandlePageFaultTlb(CoreVmcbData* vcpu, GPRegs* guest_regs)
	{
		auto fault_address = (void*)vcpu->guest_vmcb.control_area.ExitInfo2;

		PageFaultErrorCode error_code;
		error_code.as_uint32 = vcpu->guest_vmcb.control_area.ExitInfo1;

		auto hook_entry = TlbHooker::FindByPage(PAGE_ALIGN(fault_address));

		if (!hook_entry)
		{
			Logger::Log("[AMD-Hypervisor] - This is a normal page fault at %p \n", fault_address);

			InjectException(vcpu, 14, error_code.as_uint32);
			return;
		}

		Logger::Log("[AMD-Hypervisor] -This page fault is from our hooked page at %p \n", fault_address);

		if (error_code.fields.execute)
		{
			/*	Mark the page as present	*/

			auto pte = Utils::GetPte(fault_address, vcpu->guest_vmcb.save_state_area.Cr3,
				[](PT_ENTRY_64* pte) -> int {
					pte->Present = 1;
					return 0;
				}
			);

			hook_entry->ret_pointer();	/*	Add the iTLB entry	*/
			Logger::Log("[AMD-Hypervisor] - added iTLB entry! \n");

			pte->Present = 0;	/*	Make sure we can intercept memory access after iTLB miss	*/
		}
		else
		{
			auto pte = Utils::GetPte(fault_address, vcpu->guest_vmcb.save_state_area.Cr3);

			Logger::Log(
				"[AMD-Hypervisor] - Read access on hooked page, rip = 0x%p \n", 
				vcpu->guest_vmcb.save_state_area.Rip
			);

			/*	This is the page of one of our hooks, load the innocent page into dTLB	*/
			pte->Present = 1;
			pte->PageFrameNumber = hook_entry->hookless_pte->PageFrameNumber;

			auto data = *(int*)fault_address;

			pte->Present = 0;
		}
	}
};
