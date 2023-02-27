#pragma once
#include "utils.h"
#include "includes.h"
#include "disassembly.h"

namespace Hooks
{
	struct JmpRipCode
	{
		uintptr_t fn_address;
		uintptr_t target_address;

		size_t hook_size;
		size_t orig_bytes_size;
		void* original_bytes;
		uint8_t* hook_code;

		JmpRipCode(void* hook_address, void* jmp_target) 
		: fn_address ((uintptr_t)hook_address), target_address((uintptr_t)jmp_target)
		{
			hook_size = Disasm::LengthOfInstructions((void*)hook_address, 14);

			orig_bytes_size = hook_size + 14;      /*  because orig_bytes includes jmp back code   */

			auto jmp_back_location = (uintptr_t)hook_address + hook_size;

			char jmp_rip[15] = "\xFF\x25\x00\x00\x00\x00\xCC\xCC\xCC\xCC\xCC\xCC\xCC\xCC";

			original_bytes = (uint8_t*)VirtualAlloc(NULL, orig_bytes_size, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);

			memcpy(original_bytes, hook_address, hook_size);
			memcpy((uint8_t*)original_bytes + hook_size, jmp_rip, 14);
			memcpy((uint8_t*)original_bytes + hook_size + 6, &jmp_back_location, sizeof(uintptr_t));

			hook_code = (uint8_t*)VirtualAlloc(NULL, hook_size, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);

			memcpy(jmp_rip + 6, &jmp_target, sizeof(uintptr_t));
			memcpy(hook_code, jmp_rip, 14);
		}
	};
};