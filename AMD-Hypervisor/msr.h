#pragma once
#include "vmexit.h"

void HandleMsrExit(VcpuData* core_data, GeneralRegisters* guest_regs);