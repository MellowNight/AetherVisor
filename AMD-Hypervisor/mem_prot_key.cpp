#include "mem_prot_key.h"

extern "C" __int64 __rdpkru();

namespace MpkHooks
{
	void Init()
    {

    }

	MpkHook* SetMpkHook(void* address, uint8_t* patch, size_t patch_len)
    {	
        auto hook_entry = &first_mpk_hook;

		for (int i = 0; i < hook_count; hook_entry = hook_entry->next_hook, ++i)
		{
		}

		CR3 cr3;
		cr3.Flags = __readcr3();

		hook_entry->the_pte = Utils::GetPte(address, cr3.AddressOfPageDirectory << PAGE_SHIFT);
        hook_entry->hooked_page = PAGE_ALIGN(address);

        hook_entry->the_pte->ProtectionKey = hook_count;

        auto pkru = __rdpkru();

        /* Pkru ADi bit at hook_count idx  */
        auto flag = 1 << (hook_count * 2);     
        pkru = pkru | flag;     

        __wrpkru(pkru);

        hook_count += 1;
    }
}