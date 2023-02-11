
#include "aethervisor.h"
#include "utils.h"

namespace AetherVisor
{
    namespace SyscallHook
    {
        uint32_t tls_index = 0;

        int HookEFER()
        {
            tls_index = TlsAlloc();

            Util::ForEachCore(
                [](void* params) -> void {
                    svm_vmmcall(VMMCALL_ID::hook_efer_syscall, tls_index);
                }, NULL
            );

            return 0;
        }
    }
}
