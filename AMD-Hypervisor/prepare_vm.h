#pragma once
#include "npt.h"
#include "hypervisor.h"

bool IsSvmSupported();

bool IsSvmUnlocked();

void EnableSvme();

void ConfigureProcessor(
	VcpuData* core_data, 
	CONTEXT* context_record
);

bool IsCoreReadyForVmrun(
	VMCB* guest_vmcb, 
	SegmentAttribute cs_attribute
);