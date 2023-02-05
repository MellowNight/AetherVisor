
#include "aethervisor_kernel.h"
#include "utils.h"


namespace AetherVisor
{
    namespace SyscallHook
    {
        int HookEFER()
        {
            Util::ForEachCore(
                [](void* params) -> void {
                    svm_vmmcall(VMMCALL_ID::hook_efer_syscall);
                }, NULL
            );

            return 0;
        }
    }
}
