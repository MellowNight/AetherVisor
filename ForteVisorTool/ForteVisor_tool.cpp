#include <iostream>
#include "forte_api.h"

int main()
{
	svm_vmmcall(VMMCALL_ID::disable_hv);
	//ForteVisor::DisableHv();
}