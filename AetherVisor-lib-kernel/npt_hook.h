#pragma once

namespace AetherVisor
{
    namespace NptHook
    {
        int SetNptHook(
            uintptr_t address,
            uint8_t* patch,
            size_t patch_len,
            NCR3_DIRECTORIES ncr3_id = NCR3_DIRECTORIES::primary,
            bool global_page = false
        );

        int RemoveNptHook(
            uintptr_t address
        );
    }
}
