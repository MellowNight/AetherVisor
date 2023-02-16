
#include "aethervisor_kernel.h"
#include "utils.h"


namespace Aether
{
	namespace NptHook
	{
#pragma optimize( "", off )

        int Set(uintptr_t address, uint8_t* patch, size_t patch_len, NCR3_DIRECTORIES ncr3_id, bool global_page)
        {
            if (global_page)
            {
                Util::TriggerCOW((uint8_t*)address);
            }

            return svm_vmmcall(VMMCALL_ID::set_npt_hook, address, patch, patch_len, ncr3_id);
        }
#pragma optimize( "", on )

        int Remove(uintptr_t address)
        {
            return svm_vmmcall(VMMCALL_ID::remove_npt_hook, address);
        }
	}
}
