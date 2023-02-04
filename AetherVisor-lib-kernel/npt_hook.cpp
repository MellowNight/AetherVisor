#include "aethervisor.h"

namespace AetherVisor
{
	namespace NptHook
	{
#pragma optimize( "", off )

        int SetNptHook(uintptr_t address, uint8_t* patch, size_t patch_len, int32_t ncr3_id, bool global_page)
        {
            Util::TriggerCOW((uint8_t*)address);

            svm_vmmcall(VMMCALL_ID::set_npt_hook, address, patch, patch_len, ncr3_id);
        }
#pragma optimize( "", on )

        int RemoveNptHook(uintptr_t address)
        {
            svm_vmmcall(VMMCALL_ID::remove_npt_hook, address);

            return 0;
        }
	}
}
