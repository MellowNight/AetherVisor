#include "hypervisor.h"

Hypervisor* Hypervisor::hypervisor = NULL;

Hypervisor* Hypervisor::Get()
{
    if (hypervisor == 0)
    {
        hypervisor = (Hypervisor*)ExAllocatePool(NonPagedPool, sizeof(Hypervisor));
        hypervisor->Init();
    }

    return hypervisor;
}

bool Hypervisor::IsHypervisorPresent(int32_t core_number)
{
	/*	I switched from vmmcall to a simple pointer check to avoid #UD	*/

	if (hypervisor->vcpu_data[core_number] != NULL)
	{
		return true;
	}
	else
	{
		return false;
	}
}