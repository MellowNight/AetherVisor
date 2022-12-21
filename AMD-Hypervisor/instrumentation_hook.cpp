#include "instrumentation_hook.h"

namespace Instrumentation
{
	void* callbacks[4];

	BOOL InvokeHook(VcpuData* vcpu_data, HookId handler, bool is_kernel)
	{
		auto vmroot_cr3 = __readcr3();

		if (!is_kernel)
		{
			__writecr3(vcpu_data->guest_vmcb.save_state_area.Cr3.Flags);
		}

		auto guest_rip = vcpu_data->guest_vmcb.save_state_area.Rip;

		DbgPrint("guest_rip %p is_kernel %i \n", guest_rip, is_kernel);

		int callback_permission = ((uintptr_t)callbacks[handler] > 0x7FFFFFFFFFFF) ? 3 : 0;
		int rip_permission = (guest_rip > 0x7FFFFFFFFFFF) ? 3 : 0;

		if (callback_permission == rip_permission || handler == sandbox_readwrite)
		{
			vcpu_data->guest_vmcb.save_state_area.Rip = (uintptr_t)callbacks[handler];

			vcpu_data->guest_vmcb.save_state_area.Rsp -= 8;
			*(uintptr_t*)vcpu_data->guest_vmcb.save_state_area.Rsp = guest_rip;
		}
		else
		{
			DbgPrint("ADDRESS SPACE MISMATCH \n");
			__writecr3(vmroot_cr3);

			return FALSE;
		}

		vcpu_data->guest_vmcb.control_area.NCr3 = Hypervisor::Get()->ncr3_dirs[primary];

		vcpu_data->guest_vmcb.control_area.VmcbClean &= 0xFFFFFFEF;
		vcpu_data->guest_vmcb.control_area.TlbControl = 1;

		if (!is_kernel)
		{
			__writecr3(vmroot_cr3);
		}

		return TRUE;
	}
};

