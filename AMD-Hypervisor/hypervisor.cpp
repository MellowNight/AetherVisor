#include "hypervisor.h"

Hypervisor* hypervisor = NULL;

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