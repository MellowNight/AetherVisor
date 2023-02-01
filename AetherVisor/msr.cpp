#include "msr.h"

void MsrExitHandler(VcpuData* core_data, GuestRegisters* guest_regs)
{
    uint32_t msr_id = guest_regs->rcx & (uint32_t)0xFFFFFFFF;

    if (!(((msr_id > 0) && (msr_id < 0x00001FFF)) || ((msr_id > 0xC0000000) && (msr_id < 0xC0001FFF)) || (msr_id > 0xC0010000) && (msr_id < 0xC0011FFF)))
    {
        /*  PUBG and Fortnite's unimplemented MSR checks    */

        InjectException(core_data, EXCEPTION_GP_FAULT, true, 0);
        core_data->guest_vmcb.save_state_area.rip = core_data->guest_vmcb.control_area.nrip;

        return;
    }

    LARGE_INTEGER   msr_value{ msr_value.QuadPart = __readmsr(msr_id) };

    switch (msr_id)
    {
    case MSR::efer:
    {
        auto efer = (EferMsr*)&msr_value.QuadPart;

        Logger::Get()->Log(" MSR::EFER caught, msr_value.QuadPart = %p \n", msr_value.QuadPart);

        efer->svme = 0;
        break;
    }
    default:
        break;
    }

    core_data->guest_vmcb.save_state_area.rax = msr_value.LowPart;
    guest_regs->rdx = msr_value.HighPart;

    core_data->guest_vmcb.save_state_area.rip = core_data->guest_vmcb.control_area.nrip;
}


