#include "npt_setup.h"
#include "npt_hook.h"
#include "hook_handler.h"

PHYSICAL_MEMORY_RANGE   g_PhysMemRange[10];
int  numberOfRuns = 0;

void*	AllocateNewTable(PML4E_64* PageEntry)
{
	void*	Table = ExAllocatePoolZero(NonPagedPool, PAGE_SIZE, 'ENON');

	PageEntry->PageFrameNumber = MmGetPhysicalAddress(Table).QuadPart >> PAGE_SHIFT;
	PageEntry->Write = 1;
	PageEntry->Supervisor = 1;
	PageEntry->Present = 1;
	PageEntry->ExecuteDisable = 0;

	return Table;
}

void GetPhysicalMemoryRanges()
{
	numberOfRuns = 0;

	PPHYSICAL_MEMORY_RANGE MmPhysicalMemoryRange = MmGetPhysicalMemoryRanges();

	for (int number_of_runs = 0;
		(MmPhysicalMemoryRange[number_of_runs].BaseAddress.QuadPart) || (MmPhysicalMemoryRange[number_of_runs].NumberOfBytes.QuadPart);
		number_of_runs++)
	{
		g_PhysMemRange[number_of_runs] = MmPhysicalMemoryRange[number_of_runs];

		numberOfRuns += 1;
	}

	return;
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




uintptr_t	 BuildNestedPagingTables(uintptr_t* NCr3, bool execute)
{
	GetPhysicalMemoryRanges();

	PML4E_64* n_Pml4Virtual = (PML4E_64*)ExAllocatePoolZero(NonPagedPool, PAGE_SIZE, 'ENON');

	*NCr3 = MmGetPhysicalAddress(n_Pml4Virtual).QuadPart;

	DbgPrint("[SETUP] pml4 at %p \n", n_Pml4Virtual);

	for (int run = 0; run < numberOfRuns; ++run)
	{
		uintptr_t		PageCount = g_PhysMemRange[run].NumberOfBytes.QuadPart / PAGE_SIZE;
		uintptr_t		PagesBase = g_PhysMemRange[run].BaseAddress.QuadPart / PAGE_SIZE;

		for (PFN_NUMBER PFN = PagesBase; PFN < PagesBase + PageCount; ++PFN)
		{
			AssignNPTEntry(n_Pml4Virtual, PFN << PAGE_SHIFT, execute);
		}
	}

	APIC_BAR	apic_bar;

	apic_bar.Flags = __readmsr(MSR_APIC_BAR);

	AssignNPTEntry(n_Pml4Virtual, apic_bar.ApicBase << PAGE_SHIFT, true);

	return *NCr3;
}

void	InitializeHookList(GlobalHvData*	HvData)
{
	HvData->first_hook = (NptHookEntry*)ExAllocatePoolZero(NonPagedPool, sizeof(NptHookEntry), 'hook');
	HvData->hook_list_head = &HvData->first_hook->HookList;
}