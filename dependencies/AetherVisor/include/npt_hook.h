#pragma once
#include  "aethervisor_base.h"

namespace AetherVisor
{
    namespace NptHook
    {
        int Set(
            uintptr_t address,
            uint8_t* patch,
            size_t patch_len,
            NCR3_DIRECTORIES ncr3_id = NCR3_DIRECTORIES::primary,
            bool global_page = false
        );

        int Remove(uintptr_t address);
    }
}
