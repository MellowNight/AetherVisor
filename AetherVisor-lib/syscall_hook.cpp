
#include "aethervisor.h"
#include "utils.h"

namespace Aether
{
    namespace SyscallHook
    {
        // im using this as a BOOL

        void* callback_pending = FALSE;

        void Init()
        {
            int tls_idx = TlsAlloc();

            instrumentation_hooks[syscall].tls_params_idx = tls_idx;

            TlsSetValue(tls_idx, callback_pending);
        }

        int Enable()
        {
            Util::ForEachCore(
                [](void* params) -> void {
                    svm_vmmcall(VMMCALL_ID::hook_efer_syscall, TRUE);
                }, NULL
            );

            return 0;
        }
        
        int Disable()
        {
            TlsFree(instrumentation_hooks[syscall].tls_params_idx);

            Util::ForEachCore(
                [](void* params) -> void 
                {
                    svm_vmmcall(VMMCALL_ID::hook_efer_syscall, FALSE);
                }, NULL
            );

            return 0;
        }
    }
}
