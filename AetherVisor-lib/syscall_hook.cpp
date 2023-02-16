
#include "aethervisor.h"
#include "utils.h"

namespace Aether
{
    namespace SyscallHook
    {
        uint32_t tls_index = 0;

        int Enable()
        {
            tls_index = TlsAlloc();

            Util::ForEachCore(
                [](void* params) -> void {
                    svm_vmmcall(VMMCALL_ID::hook_efer_syscall, TRUE, tls_index);
                }, NULL
            );

            return 0;
        }
        
        int Disable()
        {
            TlsFree(tls_index);

            Util::ForEachCore(
                [](void* params) -> void 
                {
                    svm_vmmcall(VMMCALL_ID::hook_efer_syscall, FALSE, tls_index);
                }, NULL
            );

            return 0;
        }
    }
}
