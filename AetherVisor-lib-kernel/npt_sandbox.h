#pragma once

namespace AetherVisor
{
    namespace Sandbox
    {
        void DenySandboxMemAccess(void* page_addr, bool allow_reads);

        void DenySandboxMemAccessRange(void* base, size_t range, bool allow_reads);

        int SandboxPage(uintptr_t address, uintptr_t tag);

        void SandboxRegion(uintptr_t base, uintptr_t size);
    }
}
