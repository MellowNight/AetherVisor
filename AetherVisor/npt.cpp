#include "npt_sandbox.h"
#include "npt_hook.h"
#include "logging.h"
#include "hypervisor.h"
#include "branch_tracer.h"

#pragma optimize( "", off )

bool HandleSplitInstruction(VcpuData* vcpu, uintptr_t guest_rip, PHYSICAL_ADDRESS faulting_physical, bool is_hooked_page)
{
	PHYSICAL_ADDRESS ncr3; ncr3.QuadPart = vcpu->guest_vmcb.control_area.ncr3;

	bool switch_ncr3 = true;

	ZydisDecodedOperand operands[5];

	int insn_len = Disasm::Disassemble((uint8_t*)guest_rip, operands).length;

	/*	handle cases where an instruction is split across 2 pages (using SINGLE STEP is better here tbh)	*/

	if (PAGE_ALIGN(guest_rip + insn_len) != PAGE_ALIGN(guest_rip))
	{
		if (is_hooked_page)
		{
			Logger::Get()->LogJunk("instruction is split, entering hook page!\n");

			/*	if CPU is entering the page:	*/

			switch_ncr3 = true;

			ncr3.QuadPart = Hypervisor::Get()->ncr3_dirs[shadow];
		}
		else
		{
			Logger::Get()->LogJunk("instruction is split, leaving hook page! \n");

			/*	if CPU is leaving the page:	*/

			switch_ncr3 = false;
		}

		auto guest_cr3 = vcpu->guest_vmcb.save_state_area.cr3.Flags;

		auto page1_physical = faulting_physical.QuadPart;
		auto page2_physical = Utils::GetPte((void*)(guest_rip + insn_len), guest_cr3)->PageFrameNumber << PAGE_SHIFT;

		Utils::GetPte((void*)page1_physical, ncr3.QuadPart)->ExecuteDisable = 0;
		Utils::GetPte((void*)page2_physical, ncr3.QuadPart)->ExecuteDisable = 0;
	}

	return switch_ncr3;
}


void VcpuData::NestedPageFaultHandler(GuestRegisters* guest_regs)
{
	PHYSICAL_ADDRESS fault_physical; fault_physical.QuadPart = guest_vmcb.control_area.exit_info2;

	NestedPageFaultInfo1 exit_info1; exit_info1.as_uint64 = guest_vmcb.control_area.exit_info1;

	PHYSICAL_ADDRESS ncr3; ncr3.QuadPart = guest_vmcb.control_area.ncr3;

	auto guest_rip = guest_vmcb.save_state_area.rip;

	/*	clean ncr3 cache	*/

	guest_vmcb.control_area.vmcb_clean &= 0xFFFFFFEF;
	guest_vmcb.control_area.tlb_control = 1;

	Logger::Get()->LogJunk("[#NPF HANDLER] 	guest physical %p, guest RIP virtual %p \n", fault_physical.QuadPart, guest_vmcb.save_state_area.rip);

	if (exit_info1.fields.valid == 0)
	{
		if (ncr3.QuadPart == Hypervisor::Get()->ncr3_dirs[sandbox])
		{
			// DbgPrint("PAGE_ALIGN(fault_physical.QuadPart) = %p \n", PAGE_ALIGN(fault_physical.QuadPart));
			
			auto denied_read_page = Sandbox::ForEachHook(
				[](auto hook_entry, auto data) -> auto {

			// 		DbgPrint("PAGE_ALIGN(hook_entry->guest_physical) = %p \n", PAGE_ALIGN(hook_entry->guest_physical));

					if (data == PAGE_ALIGN(hook_entry->guest_physical) && hook_entry->denied_access)
					{
						return true;
					}
					else
					{
						return false;
					}

				}, PAGE_ALIGN(fault_physical.QuadPart)
			);

			if (denied_read_page)
			{
			//	DbgPrint("single stepping at guest_rip = %p \n", guest_rip);

				/*
					Single-step the read/write in the ncr3 that allows all pages to be executable.
					Single-stepping mode => single-step on every instruction
				*/

				BranchTracer::Pause(this);

				guest_vmcb.save_state_area.rflags.TrapFlag = 1;

				guest_vmcb.control_area.ncr3 = Hypervisor::Get()->ncr3_dirs[sandbox_single_step];

				return;
			}
		}
		
		/*	map in the missing memory (primary and hook NCR3 only)	*/

		auto pml4_base = (PML4E_64*)MmGetVirtualForPhysical(ncr3);

		if (ncr3.QuadPart == Hypervisor::Get()->ncr3_dirs[sandbox] || ncr3.QuadPart == Hypervisor::Get()->ncr3_dirs[shadow])
		{
			auto pte = AssignNptEntry((PML4E_64*)pml4_base, fault_physical.QuadPart, PTEAccess{ true, true, false });
		}
		else
		{
			auto pte = AssignNptEntry((PML4E_64*)pml4_base, fault_physical.QuadPart, PTEAccess{ true, true, true });
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
			BranchTracer::Resume(this);
		}

		if (guest_vmcb.control_area.ncr3 == Hypervisor::Get()->ncr3_dirs[sandbox])
		{
			BranchTracer::SetLBR(this, FALSE);

			ZydisDecodedOperand operands[5];

			auto insn = Disasm::Disassemble((uint8_t*)guest_vmcb.save_state_area.br_from, operands);

			auto insn_category = insn.meta.category;

			//	CHAR printBuffer[128];

			//Disasm::format(guest_vmcb.save_state_area.br_from, &insn, printBuffer);

			if (/*insn_category == ZYDIS_CATEGORY_COND_BR || insn_category == ZYDIS_CATEGORY_RET || */insn_category == ZYDIS_CATEGORY_CALL/* || insn_category == ZYDIS_CATEGORY_UNCOND_BR*/)
			{
				/*  call out of sandbox context and set RIP to the instrumentation hook for executes  */

				Instrumentation::InvokeHook(this, Instrumentation::sandbox_execute, &guest_vmcb.save_state_area.br_from, sizeof(int64_t));
			}
		}

		auto sandbox_npte = Utils::GetPte((void*)fault_physical.QuadPart, Hypervisor::Get()->ncr3_dirs[sandbox]);

		if (sandbox_npte)
		{
			if (sandbox_npte->ExecuteDisable == FALSE)
			{

				BranchTracer::SetLBR(this, TRUE);

				/*  enter into the sandbox context    */

				// DbgPrint("0x%p is a sandbox page! \n", fault_physical.QuadPart);

				guest_vmcb.control_area.ncr3 = Hypervisor::Get()->ncr3_dirs[sandbox];

				return;
			}
		}

		auto npthooked_page = Utils::GetPte((void*)fault_physical.QuadPart, Hypervisor::Get()->ncr3_dirs[shadow]);

		/*	handle cases where an instruction is split across 2 pages	*/

		auto switch_ncr3 = HandleSplitInstruction(this, guest_rip, fault_physical, !npthooked_page->ExecuteDisable);

		if (switch_ncr3)
		{
			if (!npthooked_page->ExecuteDisable)
			{
				/*  move into hooked page and switch to ncr3 with hooks mapped  */

				guest_vmcb.control_area.ncr3 = Hypervisor::Get()->ncr3_dirs[shadow];
			}
			else
			{
				guest_vmcb.control_area.ncr3 = Hypervisor::Get()->ncr3_dirs[primary];
			}
		}
	}
}

#pragma optimize( "", on )


/*	AllocateNewTable(): Allocate a new nested page table (nPML4, nPDPT, nPD, nPT) for a guest physical address translation
*/


void* AllocateNewTable(PT_ENTRY_64* page_entry)
{
	static int max_reserve = 60000;

	static void* page_table_reserves[60000];

	static bool not_reserved = true;

	static int last_reserved_count = 0;

	if (not_reserved)
	{
		max_reserve = 60000;
		last_reserved_count = 0;

		for (int i = 0; i < max_reserve; ++i)
		{
			page_table_reserves[i] = ExAllocatePoolZero(NonPagedPool, PAGE_SIZE, 'ENON');
		}

		not_reserved = false;
	}

	void* page_table = page_table_reserves[last_reserved_count];

	last_reserved_count += 1;

	if (last_reserved_count > max_reserve)
	{
		KeBugCheckEx(MANUALLY_INITIATED_CRASH, 0, 1, 2, 3);
	}

	page_entry->PageFrameNumber = MmGetPhysicalAddress(page_table).QuadPart >> PAGE_SHIFT;
	page_entry->Write = 1;
	page_entry->Supervisor = 1;
	page_entry->Present = 1;
	page_entry->ExecuteDisable = 0;


	if (page_table == NULL)
	{
		KeBugCheckEx(MANUALLY_INITIATED_CRASH, (ULONG64)page_table, last_reserved_count, NULL, NULL);
	}

	return page_table;
}


int GetPhysicalMemoryRanges()
{
	int num_of_runs = 0;

	PPHYSICAL_MEMORY_RANGE memory_range = MmGetPhysicalMemoryRanges();

	for (num_of_runs = 0;
		(memory_range[num_of_runs].BaseAddress.QuadPart) || (memory_range[num_of_runs].NumberOfBytes.QuadPart);
		num_of_runs++)
	{
		Hypervisor::Get()->phys_mem_range[num_of_runs] = memory_range[num_of_runs];
	}

	return num_of_runs;
}


/*	assign a new NPT entry to an unmapped guest physical address	*/

PTE_64*	AssignNptEntry(PML4E_64* npml4, uintptr_t physical_addr, PTEAccess flags)
{
	AddressTranslationHelper address_bits;

	address_bits.as_int64 = physical_addr;

	PML4E_64* pml4e = &npml4[address_bits.AsIndex.pml4];
	PDPTE_64* pdpt;

	if (pml4e->Present == 0)
	{
		pdpt = (PDPTE_64*)AllocateNewTable((PT_ENTRY_64*)pml4e);
	}
	else
	{
		pdpt = (PDPTE_64*)Utils::PfnToVirtualAddr(pml4e->PageFrameNumber);
	}

	PDPTE_64* pdpte = &pdpt[address_bits.AsIndex.pdpt];
	PDE_64* pd;

	if (pdpte->Present == 0)
	{
		pd = (PDE_64*)AllocateNewTable((PT_ENTRY_64*)pdpte);
	}
	else
	{
		pd = (PDE_64*)Utils::PfnToVirtualAddr(pdpte->PageFrameNumber);
	}

	PDE_64* pde = &pd[address_bits.AsIndex.pd];
	PTE_64* pt;

	if (pde->Present == 0)
	{
		pt = (PTE_64*)AllocateNewTable((PT_ENTRY_64*)pde);
	}
	else
	{
		pt = (PTE_64*)Utils::PfnToVirtualAddr(pde->PageFrameNumber);
	}

	PTE_64* pte = &pt[address_bits.AsIndex.pt];

	pte->PageFrameNumber = static_cast<PFN_NUMBER>(physical_addr >> PAGE_SHIFT);
	pte->Supervisor = 1;
	pte->Write = flags.writable;
	pte->Present = flags.present;
	pte->ExecuteDisable = !flags.execute;

	return pte;
}

uintptr_t BuildNestedPagingTables(uintptr_t* ncr3, PTEAccess flags)
{
	auto run_count = GetPhysicalMemoryRanges();

	auto npml4_virtual = (PML4E_64*)ExAllocatePoolZero(NonPagedPool, PAGE_SIZE, 'ENON');

	*ncr3 = MmGetPhysicalAddress(npml4_virtual).QuadPart;

	DbgPrint("[SETUP] npml4_virtual %p flags.present %i flags.write  %i flags.execute  %i \n", npml4_virtual, flags.present, flags.writable, flags.execute);

	/*	Create an 1:1 guest to host translation for each physical page address	*/

	for (int run = 0; run < run_count; ++run)
	{
		uintptr_t page_count = Hypervisor::Get()->phys_mem_range[run].NumberOfBytes.QuadPart / PAGE_SIZE;
		uintptr_t pages_base = Hypervisor::Get()->phys_mem_range[run].BaseAddress.QuadPart / PAGE_SIZE;

		for (PFN_NUMBER pfn = pages_base; pfn < pages_base + page_count; ++pfn)
		{
			AssignNptEntry(npml4_virtual, pfn << PAGE_SHIFT, flags);
		}
	}

	/*	APIC range isn't covered by system physical memory ranges, but it still needs to be visible	*/

	APIC_BAR_MSR apic_bar;

	apic_bar.value = __readmsr(MSR::apic_bar);

	AssignNptEntry(npml4_virtual, apic_bar.apic_base << PAGE_SHIFT, flags);

	return *ncr3;
}