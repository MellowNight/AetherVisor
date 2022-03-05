#include "npt_hook.h"
#include "itlb_hook.h"
#include "logging.h"
#include "hypervisor.h"

uintptr_t GuestRip = 0;

void HandleNestedPageFault(CoreVmcbData* VpData, GPRegs* GuestContext)
{
	NestedPageFaultInfo1 exit_info1 = { VpData->guest_vmcb.control_area.ExitInfo1 };

	uintptr_t fail_address = VpData->guest_vmcb.control_area.ExitInfo2;

	PHYSICAL_ADDRESS NCr3;

	NCr3.QuadPart = VpData->guest_vmcb.control_area.NCr3;

	GuestRip = VpData->guest_vmcb.save_state_area.Rip;


	if (exit_info1.fields.valid == 0)
	{
		int num_bytes = VpData->guest_vmcb.control_area.NumOfBytesFetched;

		auto insn_bytes = VpData->guest_vmcb.control_area.GuestInstructionBytes;

		auto pml4_base = (PML4E_64*)MmGetVirtualForPhysical(NCr3);

		auto pte = AssignNPTEntry((PML4E_64*)pml4_base, fail_address, true);

		return;
	}

	if (exit_info1.fields.execute == 1)
	{
		auto nptHook = NptHooks::FindByHooklessPhysicalPage(fail_address);

		bool Switch = true;

		int length = 16;

		/*	handle cases where an instruction is split across 2 pages	*/

		if (PAGE_ALIGN(GuestRip + length) != PAGE_ALIGN(GuestRip)) 
		{
			PT_ENTRY_64* N_Pte = Utils::GetPte((void*)
				MmGetPhysicalAddress(PAGE_ALIGN(GuestRip)).QuadPart, NCr3.QuadPart);

			PT_ENTRY_64* N_Pte2 = Utils::GetPte((void*)
				MmGetPhysicalAddress(PAGE_ALIGN(GuestRip + length)).QuadPart, NCr3.QuadPart);


			N_Pte->ExecuteDisable = 0;
			N_Pte2->ExecuteDisable = 0;

			Switch = false;
		}

		/*	clean ncr3 cache	*/

		VpData->guest_vmcb.control_area.VmcbClean &= ~(1 << 4);
		//VpData->guest_vmcb.control_area.TlbControl = 3;

		/*  switch to hook CR3, with hooks mapped or switch to innocent CR3, without any hooks  */
		if (Switch)
		{
			if (nptHook)
			{
				VpData->guest_vmcb.control_area.NCr3 = hypervisor->noexecute_ncr3;
			}
			else
			{
				VpData->guest_vmcb.control_area.NCr3 = hypervisor->normal_ncr3;
			}
		}
	}
}

/*	guest physical addresses are 1:1 mapped to host physical
	addresses
*/

void* AllocateNewTable(PML4E_64* PageEntry)
{
	void* Table = ExAllocatePoolZero(NonPagedPool, PAGE_SIZE, 'ENON');

	PageEntry->PageFrameNumber = MmGetPhysicalAddress(Table).QuadPart >> PAGE_SHIFT;
	PageEntry->Write = 1;
	PageEntry->Supervisor = 1;
	PageEntry->Present = 1;
	PageEntry->ExecuteDisable = 0;

	return Table;
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