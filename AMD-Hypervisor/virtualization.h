#pragma once
#include "npt_setup.h"
#include "npt_hook.h"

bool	IsSvmSupported();
bool	IsSvmUnlocked();
void	EnableSvme();
void	ConfigureProcessor(CoreVmcbData* core_data, CONTEXT* context_record);
bool	IsProcessorReadyForVmrun(Vmcb* guest_vmcb, SegmentAttribute cs_attribute);