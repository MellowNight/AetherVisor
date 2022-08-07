#include "npthook_safety.h"
#include "portable_executable.h"
#include "vmexit.h"

namespace NptHooks
{
	int hook_count;
	NptHook first_npt_hook;

	Hooks::JmpRipCode hk_NtTerminateProcess;
	Hooks::JmpRipCode hk_MmCleanProcessAddressSpace;

	__int64 __fastcall NtTerminateProcess_hook(__int64 a1, __int64 a2)
	{
		/*	unset all NPT hooks for this process	*/

		ForEachHook(
			[](NptHook* hook_entry, void* data)->bool {

				if (hook_entry->process_cr3 == (uintptr_t)data)
				{
					UnsetHook(hook_entry);
				}

				return false;
			}, 
			(void*)__readcr3()
		);

		return static_cast<decltype(&NtTerminateProcess_hook)>(hk_NtTerminateProcess.original_bytes)(a1, a2);
	}

	char __fastcall MmCleanProcessAddressSpace_hook(__int64 a1, __int64 a2)
	{
		/*	unset all NPT hooks for this process	*/

		ForEachHook(
			[](NptHook* hook_entry, void* data)-> bool {

				if (hook_entry->process_cr3 == (uintptr_t)data)
				{
					UnsetHook(hook_entry);
				}

				return false;
			},
			(void*)__readcr3()
		);

		return static_cast<decltype(&MmCleanProcessAddressSpace_hook)>(hk_MmCleanProcessAddressSpace.original_bytes)(a1, a2);
	}

	void PageSynchronizationPatch()
	{
		/*	place a callback on NtTerminateProcess to remove npt hooks inside terminating processes, to prevent PFN check bsods	*/

		ULONG nt_size = NULL;
		auto ntoskrnl = (uintptr_t)Utils::GetKernelModule(&nt_size, RTL_CONSTANT_STRING(L"ntoskrnl.exe"));

		auto pe_hdr = PeHeader(ntoskrnl);

		auto section = (IMAGE_SECTION_HEADER*)(pe_hdr + 1);

		for (int i = 0; i < pe_hdr->FileHeader.NumberOfSections; ++i)
		{
			/*	NtTerminateProcess & MmCleanProcessAddressSpace hook to clean up NPT hooks after process exit	*/

			if (!strcmp((char*)section[i].Name, "PAGE"))
			{
				uint8_t* start = section[i].VirtualAddress + (uint8_t*)ntoskrnl;

				uintptr_t terminate_process = NULL;
				uintptr_t clean_process_address_space = NULL;

				terminate_process = Utils::FindPattern((uintptr_t)start, section[i].Misc.VirtualSize, "\x4C\x8B\xDC\x49\x89\x5B\x10\x49\x89\x6B\x18\x57", 12, 0x00);
				clean_process_address_space = RELATIVE_ADDR(Utils::FindPattern((uintptr_t)start, section[i].Misc.VirtualSize, "\xE8\xCC\xCC\xCC\xCC\x8B\x5D\x00", 8, 0xCC), 1, 5);

				hk_NtTerminateProcess = Hooks::JmpRipCode{ terminate_process, (uintptr_t)NtTerminateProcess_hook };
				hk_MmCleanProcessAddressSpace = Hooks::JmpRipCode{ clean_process_address_space, (uintptr_t)MmCleanProcessAddressSpace_hook };

				LARGE_INTEGER length_tag;
				length_tag.LowPart = NULL;
				length_tag.HighPart = hk_NtTerminateProcess.hook_size;

				svm_vmmcall(VMMCALL_ID::set_npt_hook, terminate_process, hk_NtTerminateProcess.hook_code, length_tag.QuadPart);
				svm_vmmcall(VMMCALL_ID::set_npt_hook, clean_process_address_space, hk_MmCleanProcessAddressSpace.hook_code, length_tag.QuadPart);
			}

			/*	patch out MEMORY_MANAGEMENT bugcheck	*/

			if (!strcmp((char*)section[i].Name, ".text"))
			{
				uint8_t* start = section[i].VirtualAddress + (uint8_t*)ntoskrnl;
				uint8_t* end = section[i].Misc.VirtualSize + start;

				DbgPrint("start %p \n", start);
				DbgPrint("ntoskrnl_base %p \n", ntoskrnl);


				Disasm::ForEachInstruction(start, end, [](uint8_t* insn_addr, ZydisDecodedInstruction insn) -> void {

					ZydisDecodedOperand operands[ZYDIS_MAX_OPERAND_COUNT_VISIBLE];

					if (insn.mnemonic == ZYDIS_MNEMONIC_JNZ)
					{
						Disasm::Disassemble((uint8_t*)insn_addr, operands);

						auto jmp_target = Disasm::GetJmpTarget(insn, operands, (ZyanU64)insn_addr);

						size_t instruction_size = NULL;

						for (auto instruction = jmp_target; instruction < jmp_target + 0x40; instruction = instruction + instruction_size)
						{
							ZydisDecodedOperand operands[ZYDIS_MAX_OPERAND_COUNT_VISIBLE];

							auto insn2 = Disasm::Disassemble((uint8_t*)instruction, operands);

							if (insn2.mnemonic == ZYDIS_MNEMONIC_MOV)
							{
								// mov edx, 403h MEMORY_MANAGEMENT page sync

								if (operands[1].imm.value.u == 0x403 || operands[1].imm.value.u == 0x411 || operands[1].imm.value.u == 0x888a)
								{
									DbgPrint("found MEMORY_MANAGEMENT jnz at %p \n", insn_addr);

									LARGE_INTEGER length_tag;
									length_tag.LowPart = NULL;
									length_tag.HighPart = 6;

									svm_vmmcall(VMMCALL_ID::set_npt_hook, insn_addr, "\x90\x90\x90\x90\x90\x90", length_tag.QuadPart);
								}
							}

							instruction_size = insn2.length;
						}
					}
					});

				/*	There is a patch location that Disasm::ForEachInstruction fails to find, and I can't be bothered to fix that shit!	;)	*/

				auto patch_loc = Utils::FindPattern((uintptr_t)start, section[i].Misc.VirtualSize, "\x48\x85\x56\x28\x0F\x84", 6, 0x00) + 4;
				svm_vmmcall(VMMCALL_ID::set_npt_hook, patch_loc, "\x90\x90\x90\x90\x90\x90", 6);
			}
		}
	}
};
