
#include "aethervisor.h"
#include "utils.h"

namespace Aether
{
    namespace SyscallHook
    {
        TlsParams* tracer_params;

        void Init()
        {
            instrumentation_hooks[syscall].tls_params_idx = TlsAlloc();

            tracer_params = new TlsParams{ false };
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
