#include "vmexit.h"

void VcpuData::MsrExitHandler(GuestRegisters* guest_regs)
{
    uint32_t msr_id = guest_regs->rcx & (uint32_t)0xFFFFFFFF;

    if (!(((msr_id > 0) && (msr_id < 0x00001FFF)) || ((msr_id > 0xC0000000) && (msr_id < 0xC0001FFF)) || (msr_id > 0xC0010000) && (msr_id < 0xC0011FFF)))
    {
        /*  PUBG and Fortnite's unimplemented MSR checks    */

        InjectException(EXCEPTION_VECTOR::GeneralProtection, true, 0);
        guest_vmcb.save_state_area.rip = guest_vmcb.control_area.nrip;

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

    guest_vmcb.save_state_area.rax = msr_value.LowPart;
    guest_regs->rdx = msr_value.HighPart;

    guest_vmcb.save_state_area.rip = guest_vmcb.control_area.nrip;
}
//
//void SetupMSRPM(VcpuData* vcpu)
//{
//    size_t bits_per_msr = 16000 / 8000;
//    size_t bits_per_byte = sizeof(uint8_t) * 8;
//    size_t msrpm_size = PAGE_SIZE * 2;
//
//    auto msrpm = ExAllocatePoolZero(NonPagedPool, msrpm_size, 'msr0');
//
//    vcpu->guest_vmcb.control_area.msrpm_base_pa = MmGetPhysicalAddress(msrpm).QuadPart;
//
//    RTL_BITMAP bitmap;
//
//    RtlInitializeBitMap(&bitmap, (PULONG)msrpm, msrpm_size * 8);
//
//    RtlClearAllBits(&bitmap);
//
//    auto section2_offset = (0x800 * bits_per_byte);
//
//    auto efer_offset = section2_offset + (bits_per_msr * (MSR::efer - 0xC0000000));
//
//    /*	intercept EFER read and write	*/
//
//    RtlSetBits(&bitmap, efer_offset, 2);
//}