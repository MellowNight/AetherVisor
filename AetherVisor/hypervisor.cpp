#include "hypervisor.h"

Hypervisor* Hypervisor::hypervisor = NULL;

Hypervisor* Hypervisor::Get()
{
    if (hypervisor == NULL)
    {
        hypervisor = (Hypervisor*)ExAllocatePool(NonPagedPool, sizeof(Hypervisor));
        hypervisor->Init();
    }

    return hypervisor;
}

bool Hypervisor::IsCoreVirtualized(int32_t core_number)
{
    /*	I switched from vmmcall to a simple pointer check to avoid #UD	*/

    return hypervisor->vcpus[core_number] != NULL ? true : false;
}

