#include "npt_sandbox.h"
#include "npt_hook.h"
#include "logging.h"
#include "hypervisor.h"
#include "paging_utils.h"
#include "branch_tracer.h"

#pragma optimize( "", off )

bool HandleSplitInstruction(VcpuData* vcpu_data, uintptr_t guest_rip, PHYSICAL_ADDRESS faulting_physical, BOOL is_hooked_page)
{
	PHYSICAL_ADDRESS ncr3;

	ncr3.QuadPart = vcpu_data->guest_vmcb.control_area.ncr3;

	bool switch_ncr3 = true;

	int insn_len = 10;

	/*	handle cases where an instruction is split across 2 pages (using single step is better here tbh)	*/

	if (PAGE_ALIGN(guest_rip + insn_len) != PAGE_ALIGN(guest_rip))
	{
		if (is_hooked_page)
		{
			Logger::Get()->LogJunk("instruction is split, entering hook page!\n");

			/*	if CPU is entering the page:	*/

			switch_ncr3 = true;

			ncr3.QuadPart = Hypervisor::Get()->ncr3_dirs[noexecute];
		}
		else
		{
			Logger::Get()->LogJunk("instruction is split, leaving hook page! \n");

			/*	if CPU is leaving the page:	*/

			switch_ncr3 = false;
		}
		auto guest_cr3 = vcpu_data->guest_vmcb.save_state_area.cr3.Flags;

		auto page1_physical = faulting_physical.QuadPart;
		auto page2_physical = PageUtils::GetPte((void*)(guest_rip + insn_len), guest_cr3)->PageFrameNumber << PAGE_SHIFT;

		PageUtils::GetPte((void*)page1_physical, ncr3.QuadPart)->ExecuteDisable = 0;
		PageUtils::GetPte((void*)page2_physical, ncr3.QuadPart)->ExecuteDisable = 0;
	}

	return switch_ncr3;
}


void HandleNestedPageFault(VcpuData* vcpu_data, GuestRegisters* guest_registers)
{
	PHYSICAL_ADDRESS faulting_physical;
	faulting_physical.QuadPart = vcpu_data->guest_vmcb.control_area.ExitInfo2;

	NestedPageFaultInfo1 exit_info1;
	exit_info1.as_uint64 = vcpu_data->guest_vmcb.control_area.ExitInfo1;

	PHYSICAL_ADDRESS ncr3;
	ncr3.QuadPart = vcpu_data->guest_vmcb.control_area.ncr3;

	auto guest_rip = vcpu_data->guest_vmcb.save_state_area.rip;

	/*	clean ncr3 cache	*/

	vcpu_data->guest_vmcb.control_area.VmcbClean &= 0xFFFFFFEF;
	vcpu_data->guest_vmcb.control_area.TlbControl = 1;

	Logger::Get()->LogJunk("[#NPF HANDLER] 	guest physical %p, guest RIP virtual %p \n", faulting_physical.QuadPart, vcpu_data->guest_vmcb.save_state_area.rip);

	if (exit_info1.fields.valid == 0)
	{
		auto denied_read_page = Sandbox::ForEachHook(
			[](auto hook_entry, auto data) -> auto {

				if (data == hook_entry->guest_physical && hook_entry->unreadable)
				{
					return true;
				}
				else
				{
					return false;
				}
			}, PAGE_ALIGN(faulting_physical.QuadPart)
		);

		if (denied_read_page && ncr3.QuadPart == Hypervisor::Get()->ncr3_dirs[sandbox])
		{
			DbgPrint("single stepping at guest_rip = %p \n", guest_rip);

			/*
				Single-step the read/write in the ncr3 that allows all pages to be executable.
				Single-stepping mode => single-step on every instruction
			*/

			BranchTracer::Pause(vcpu_data);

			vcpu_data->guest_vmcb.save_state_area.rflags.TrapFlag = 1;

			vcpu_data->guest_vmcb.control_area.ncr3 = Hypervisor::Get()->ncr3_dirs[sandbox_single_step];
		}
		else
		{
			/*	map in the missing memory (primary and hook NCR3 only)	*/

			auto pml4_base = (PML4E_64*)MmGetVirtualForPhysical(ncr3);

			auto pte = AssignNPTEntry((PML4E_64*)pml4_base, faulting_physical.QuadPart, PTEAccess{ true, true, true });
		}

		return;
	}

	if (exit_info1.fields.execute == 1)
	{
		if (guest_rip > BranchTracer::range_base && guest_rip < (BranchTracer::range_size + BranchTracer::range_base))
		{
			/*  Resume the branch tracer after an NCR3 switch, if the tracer is active.
				Single-stepping mode => only #DB on branches
			*/
			BranchTracer::Resume(vcpu_data);
		}

		if (vcpu_data->guest_vmcb.control_area.ncr3 == Hypervisor::Get()->ncr3_dirs[sandbox])
		{
			/*  call out of sandbox context and set RIP to the instrumentation hook for executes  */

			auto is_system_page = (vcpu_data->guest_vmcb.save_state_area.cr3.Flags == __readcr3()) ? true : false;

			Instrumentation::InvokeHook(vcpu_data, Instrumentation::sandbox_execute, is_system_page);
		}

		auto sandbox_npte = PageUtils::GetPte((void*)faulting_physical.QuadPart, Hypervisor::Get()->ncr3_dirs[sandbox]);

		if (sandbox_npte->ExecuteDisable == FALSE)
		{
			/*  enter into the sandbox context	*/

			// DbgPrint("0x%p is a sandbox page! \n", faulting_physical.QuadPart);

			vcpu_data->guest_vmcb.control_area.ncr3 = Hypervisor::Get()->ncr3_dirs[sandbox];

			return;
		}

		auto npthooked_page = PageUtils::GetPte((void*)faulting_physical.QuadPart, Hypervisor::Get()->ncr3_dirs[noexecute]);

		/*	handle cases where an instruction is split across 2 pages	*/

		auto switch_ncr3 = HandleSplitInstruction(vcpu_data, guest_rip, faulting_physical, !npthooked_page->ExecuteDisable);

		if (switch_ncr3)
		{
			if (!npthooked_page->ExecuteDisable)
			{
				/*  move into hooked page and switch to ncr3 with hooks mapped  */

				vcpu_data->guest_vmcb.control_area.ncr3 = Hypervisor::Get()->ncr3_dirs[noexecute];
			}
			else
			{
				vcpu_data->guest_vmcb.control_area.ncr3 = Hypervisor::Get()->ncr3_dirs[primary];
			}
		}
	}
}

#pragma optimize( "", on )


/*	
	gPTE pfn would be equal to nPTE pfn at the beginning,
	because both entries are pointing to the same physical page.

	The nPTE pfn will map a different physical address when an NPT hook is set.
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
		Hypervisor::Get()->phys_mem_range[number_of_runs] = MmPhysicalMemoryRange[number_of_runs];
	}

	return number_of_runs;
}


/*	assign a new NPT entry to an unmapped guest physical address	*/

PTE_64*	AssignNPTEntry(PML4E_64* n_Pml4, uintptr_t PhysicalAddr, PTEAccess flags)
{
	AddressTranslationHelper	Helper;

	Helper.AsUInt64 = PhysicalAddr;

	PML4E_64* Pml4e = &n_Pml4[Helper.AsIndex.Pml4];
	PDPTE_64* Pdpt;

	if (Pml4e->Present == 0)
	{
		Pdpt = (PDPTE_64*)AllocateNewTable(Pml4e);
	}
	else
	{
		Pdpt = (PDPTE_64*)PageUtils::VirtualAddrFromPfn(Pml4e->PageFrameNumber);
	}


	PDPTE_64* Pdpte = &Pdpt[Helper.AsIndex.Pdpt];
	PDE_64* Pd;

	if (Pdpte->Present == 0)
	{
		Pd = (PDE_64*)AllocateNewTable((PML4E_64*)Pdpte);
	}
	else
	{
		Pd = (PDE_64*)PageUtils::VirtualAddrFromPfn(Pdpte->PageFrameNumber);
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
		Pt = (PTE_64*)PageUtils::VirtualAddrFromPfn(Pde->PageFrameNumber);
	}

	PTE_64* Pte = &Pt[Helper.AsIndex.Pt];

	Pte->PageFrameNumber = static_cast<PFN_NUMBER>(PhysicalAddr >> PAGE_SHIFT);
	Pte->Supervisor = 1;
	Pte->Write = flags.writable;
	Pte->Present = flags.present;
	Pte->ExecuteDisable = !flags.execute;

	return Pte;
}

uintptr_t BuildNestedPagingTables(uintptr_t* ncr3, PTEAccess flags)
{
	auto run_count = GetPhysicalMemoryRanges();

	auto npml4_virtual = (PML4E_64*)ExAllocatePoolZero(NonPagedPool, PAGE_SIZE, 'ENON');

	*ncr3 = MmGetPhysicalAddress(npml4_virtual).QuadPart;

	DbgPrint("[SETUP] pml4 at %p flags.present %i flags.write  %i flags.execute  %i \n", npml4_virtual, flags.present, flags.writable, flags.execute);

	for (int run = 0; run < run_count; ++run)
	{
		uintptr_t PageCount = Hypervisor::Get()->phys_mem_range[run].NumberOfBytes.QuadPart / PAGE_SIZE;
		uintptr_t PagesBase = Hypervisor::Get()->phys_mem_range[run].BaseAddress.QuadPart / PAGE_SIZE;

		for (PFN_NUMBER PFN = PagesBase; PFN < PagesBase + PageCount; ++PFN)
		{
			AssignNPTEntry(npml4_virtual, PFN << PAGE_SHIFT, flags);
		}
	}

	ApicBarMsr apic_bar;

	apic_bar.Flags = __readmsr(MSR::apic_bar);

	AssignNPTEntry(npml4_virtual, apic_bar.apic_base << PAGE_SHIFT, flags);

	return *ncr3;
}