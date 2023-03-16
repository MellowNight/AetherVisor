#include "hypervisor.h"
#include "utils.h"

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

bool VcpuData::IsPagePresent(void* address)
{
    auto guest_pte = Utils::GetPte((void*)address, __readcr3());

    if (guest_pte == NULL)
    {
        guest_vmcb.save_state_area.cr2 = (uintptr_t)address;

        InjectException(EXCEPTION_VECTOR::PageFault, true, NULL);

        suppress_nrip_increment = TRUE;

        return false;
    }

    return true;
}
