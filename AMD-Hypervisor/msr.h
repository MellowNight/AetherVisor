#pragma once
#include "vmexit.h"

void HandleMsrExit(VcpuData* core_data, GuestRegisters* guest_regs);