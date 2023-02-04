#pragma once
#include  "aethervisor_base.h"

namespace AetherVisor
{
    namespace Sandbox
    {
        void DenyPageAccess(void* page_addr, bool allow_reads);

        void DenyRegionAccess(void* base, size_t range, bool allow_reads);

        int SandboxPage(uintptr_t address, uintptr_t tag);

        void SandboxRegion(uintptr_t base, uintptr_t size);
    }
}
