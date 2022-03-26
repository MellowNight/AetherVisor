#include "npt_hook.h"
#include "itlb_hook.h"
#include "logging.h"
#include "hypervisor.h"

void HandleNestedPageFault(CoreVmcbData* vcpu_data, GPRegs* GuestContext)
{
	PHYSICAL_ADDRESS faulting_physical;
	faulting_physical.QuadPart = vcpu_data->guest_vmcb.control_area.ExitInfo2;

	NestedPageFaultInfo1 exit_info1;
	exit_info1.as_uint64 = vcpu_data->guest_vmcb.control_area.ExitInfo1;

	PHYSICAL_ADDRESS NCr3;
	NCr3.QuadPart = vcpu_data->guest_vmcb.control_area.NCr3;

	auto guest_rip = vcpu_data->guest_vmcb.save_state_area.Rip;

	if (exit_info1.fields.valid == 0)
	{
		int num_bytes = vcpu_data->guest_vmcb.control_area.NumOfBytesFetched;

		auto insn_bytes = vcpu_data->guest_vmcb.control_area.GuestInstructionBytes;

		auto pml4_base = (PML4E_64*)MmGetVirtualForPhysical(NCr3);

		auto pte = AssignNPTEntry((PML4E_64*)pml4_base, faulting_physical.QuadPart, true);

		return;
	}

	if (exit_info1.fields.execute == 1)
	{
		auto npt_hook = NptHooks::ForEachHook(
			[](NptHooks::NptHook* hook_entry, void* data) -> bool {
				
				if (PAGE_ALIGN(data) == PAGE_ALIGN(hook_entry->guest_phys_addr))
				{
					return true;
				}

			}, (void*)faulting_physical.QuadPart
		);

		Logger::Log("guest RIP physical %p, guest RIP virtual %p \n", faulting_physical.QuadPart, vcpu_data->guest_vmcb.save_state_area.Rip);

		bool switch_ncr3 = true;

		int insn_len = 10;

		/*	handle cases where an instruction is split across 2 pages	*/

		if (PAGE_ALIGN(guest_rip + insn_len) != PAGE_ALIGN(guest_rip))
		{
			if (npt_hook)
			{
				Logger::Log("instruction is split, entering hook page! \n");

				/*	if CPU is entering the page:	*/

				switch_ncr3 = true;
				NCr3.QuadPart = hypervisor->noexecute_ncr3;
			}
			else
			{
				Logger::Log("instruction is split, leaving hook page! \n");

				/*	if CPU is leaving the page:	*/

				switch_ncr3 = false;
			}

			auto page1 = faulting_physical.QuadPart;
			auto page2 = Utils::GetPte((void*)(guest_rip + insn_len), vcpu_data->guest_vmcb.save_state_area.Cr3);

			auto npte1 = Utils::GetPte((void*)page1, NCr3.QuadPart);
			auto npte2 = Utils::GetPte((void*)page2, NCr3.QuadPart);

			npte1->ExecuteDisable = 0;
			npte2->ExecuteDisable = 0;
		}

		Logger::Log("npt_hook = %p, switch_ncr3 = %d, GuestRip = %p, RSP = %p \n", npt_hook, switch_ncr3, guest_rip, vcpu_data->guest_vmcb.save_state_area.Rsp);

		/*	clean ncr3 cache	*/

		vcpu_data->guest_vmcb.control_area.VmcbClean &= 0xFFFFFFEF;
		vcpu_data->guest_vmcb.control_area.TlbControl = 1;
		//Logger::Log("===============\n");

		//Logger::Log("npt_hook found = %p guest RIP = %p \n", npt_hook, GuestRip);
		//Logger::Log("(PAGE_ALIGN(guest_rip + insn_len) = %p PAGE_ALIGN(guest_rip) = %p \n", PAGE_ALIGN(guest_rip + insn_len), PAGE_ALIGN(guest_rip));
		
		/*  switch to hook CR3, with hooks mapped or switch to innocent CR3, without any hooks  */
		if (switch_ncr3)
		{
			if (npt_hook)
			{
				vcpu_data->guest_vmcb.control_area.NCr3 = hypervisor->noexecute_ncr3;
			}
			else
			{
				vcpu_data->guest_vmcb.control_area.NCr3 = hypervisor->normal_ncr3;
			}
		}
	}
}

/*	gPTE pfn would be equal to nPTE pfn,
	because guest physical addresses are 1:1 mapped to host physical
*/
void* AllocateNewTable(PML4E_64* PageEntry)
{
	void* page_table = ExAllocatePoolZero(NonPagedPool, PAGE_SIZE, 'ENON');

	PageEntry->PageFrameNumber = MmGetPhysicalAddress(page_table).QuadPart >> PAGE_SHIFT;
	PageEntry->Write = 1;
	PageEntry->Supervisor = 1;
	PageEntry->Present = 1;
	PageEntry->ExecuteDisable = 0;

	return page_table;
}

int GetPhysicalMemoryRanges()
{
	int number_of_runs = 0;

	PPHYSICAL_MEMORY_RANGE MmPhysicalMemoryRange = MmGetPhysicalMemoryRanges();

	for (number_of_runs = 0;
		(MmPhysicalMemoryRange[number_of_runs].BaseAddress.QuadPart) || (MmPhysicalMemoryRange[number_of_runs].NumberOfBytes.QuadPart);
		number_of_runs++)
	{
		hypervisor->phys_mem_range[number_of_runs] = MmPhysicalMemoryRange[number_of_runs];
	}

	return number_of_runs;
}

/*	assign a new NPT entry to an unmapped guest physical address	*/

PTE_64*	AssignNPTEntry(PML4E_64* n_Pml4, uintptr_t PhysicalAddr, bool execute)
{
	ADDRESS_TRANSLATION_HELPER	Helper;
	Helper.AsUInt64 = PhysicalAddr;

	PML4E_64* Pml4e = &n_Pml4[Helper.AsIndex.Pml4];
	PDPTE_64* Pdpt;

	if (Pml4e->Present == 0)
	{
		Pdpt = (PDPTE_64*)AllocateNewTable(Pml4e);
	}
	else
	{
		Pdpt = (PDPTE_64*)Utils::VirtualAddrFromPfn(Pml4e->PageFrameNumber);
	}


	PDPTE_64* Pdpte = &Pdpt[Helper.AsIndex.Pdpt];
	PDE_64* Pd;

	if (Pdpte->Present == 0)
	{
		Pd = (PDE_64*)AllocateNewTable((PML4E_64*)Pdpte);
	}
	else
	{
		Pd = (PDE_64*)Utils::VirtualAddrFromPfn(Pdpte->PageFrameNumber);
	}


	PDE_64* Pde = &Pd[Helper.AsIndex.Pd];
	PTE_64* Pt;

	if (Pde->Present == 0)
	{
		Pt = (PTE_64*)AllocateNewTable((PML4E_64*)Pde);

		PTE_64* Pte = &Pt[Helper.AsIndex.Pt];
	}
	else
	{
		Pt = (PTE_64*)Utils::VirtualAddrFromPfn(Pde->PageFrameNumber);
	}

	PTE_64* Pte = &Pt[Helper.AsIndex.Pt];

	Pte->PageFrameNumber = static_cast<PFN_NUMBER>(PhysicalAddr >> PAGE_SHIFT);
	Pte->Supervisor = 1;
	Pte->Write = 1;
	Pte->Present = 1;
	Pte->ExecuteDisable = !execute;

	return Pte;
}

uintptr_t BuildNestedPagingTables(uintptr_t* NCr3, bool execute)
{
	auto numberOfRuns = GetPhysicalMemoryRanges();

	PML4E_64* n_Pml4Virtual = (PML4E_64*)ExAllocatePoolZero(NonPagedPool, PAGE_SIZE, 'ENON');

	*NCr3 = MmGetPhysicalAddress(n_Pml4Virtual).QuadPart;

	Logger::Log("[SETUP] pml4 at %p \n", n_Pml4Virtual);

	for (int run = 0; run < numberOfRuns; ++run)
	{
		uintptr_t PageCount = hypervisor->phys_mem_range[run].NumberOfBytes.QuadPart / PAGE_SIZE;
		uintptr_t PagesBase = hypervisor->phys_mem_range[run].BaseAddress.QuadPart / PAGE_SIZE;

		for (PFN_NUMBER PFN = PagesBase; PFN < PagesBase + PageCount; ++PFN)
		{
			AssignNPTEntry(n_Pml4Virtual, PFN << PAGE_SHIFT, execute);
		}
	}

	MsrApicBar apic_bar;

	apic_bar.Flags = __readmsr(MSR::APIC_BAR);

	AssignNPTEntry(n_Pml4Virtual, apic_bar.ApicBase << PAGE_SHIFT, true);

	return *NCr3;
}