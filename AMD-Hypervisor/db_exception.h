#pragma once
#include "vmexit.h"

void HandleDebugException(VcpuData* vcpu_data, GeneralRegisters* guest_ctx);
