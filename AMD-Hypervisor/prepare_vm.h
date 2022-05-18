#pragma once
#include "npt.h"
#include "itlb_hook.h"
#include "hypervisor.h"

bool IsSvmSupported();

bool IsSvmUnlocked();

void EnableSvme();

void ConfigureProcessor(
	CoreData* core_data, 
	CONTEXT* context_record
);

bool IsProcessorReadyForVmrun(
	VMCB* guest_vmcb, 
	SegmentAttribute cs_attribute
);