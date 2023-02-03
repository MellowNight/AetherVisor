#pragma once
#include "npt.h"
#include "hypervisor.h"

bool IsSvmSupported();

bool IsSvmUnlocked();

void EnableSvme();

bool IsCoreReadyForVmrun(
	VMCB* guest_vmcb, 
	SegmentAttribute cs_attribute
);