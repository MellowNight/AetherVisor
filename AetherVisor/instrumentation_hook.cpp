#include "instrumentation_hook.h"

namespace Instrumentation
{
	void* callbacks[max_id];

	bool InvokeHook(VcpuData* vcpu, CALLBACK_ID handler)
	{
		auto vmroot_cr3 = __readcr3();

		__writecr3(vcpu->guest_vmcb.save_state_area.cr3.Flags);

		auto guest_rip = vcpu->guest_vmcb.save_state_area.rip;

		DbgPrint("guest_rip %p \n", guest_rip);

		int callback_cpl = ((uintptr_t)callbacks[handler] > 0x7FFFFFFFFFFF) ? 3 : 0;

		int rip_privilege = (guest_rip > 0x7FFFFFFFFFFF) ? 3 : 0;

		if (callback_cpl == rip_privilege || handler == sandbox_readwrite)
		{
			vcpu->guest_vmcb.save_state_area.rip = (uintptr_t)callbacks[handler];

			vcpu->guest_vmcb.save_state_area.rsp -= 8;

			*(uintptr_t*)vcpu->guest_vmcb.save_state_area.rsp = guest_rip;
		}
		else
		{
			// DbgPrint("ADDRESS SPACE MISMATCH \n");
			__writecr3(vmroot_cr3);

			return FALSE;
		}

		vcpu->guest_vmcb.control_area.ncr3 = Hypervisor::Get()->ncr3_dirs[primary];

		vcpu->guest_vmcb.control_area.vmcb_clean &= 0xFFFFFFEF;
		vcpu->guest_vmcb.control_area.tlb_control = 1;

		__writecr3(vmroot_cr3);

		return TRUE;
	}
};

