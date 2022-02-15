#include "global.h"

void SetupAmdSev()
{
	int32_t	cpu_info[4] = { 0 };

	__cpuid(cpu_info, CPUID::MEMORY_ENCRYPTION_FEATURES);


}

void SetupAmdSevSnp()
{

}