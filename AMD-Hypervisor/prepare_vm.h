#pragma once
#include "npt.h"
#include "itlb_hook.h"
#include "hypervisor.h"

bool IsSvmSupported();

bool IsSvmUnlocked();

void EnableSvme();

void ConfigureProcessor(
	CoreVmcbData* core_data, 
	CONTEXT* context_record
);

bool IsProcessorReadyForVmrun(
	Vmcb* guest_vmcb, 
	SegmentAttribute cs_attribute
);