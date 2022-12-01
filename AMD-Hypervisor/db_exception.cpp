#include "db_exception.h"
#include "npt_sandbox.h"
#include "branch_tracer.h"

void HandleDebugException(VcpuData* vcpu_data)
{
    auto vmroot_cr3 = __readcr3();

    __writecr3(vcpu_data->guest_vmcb.save_state_area.Cr3);

    auto guest_rip = vcpu_data->guest_vmcb.save_state_area.Rip;

    auto lbr_stack = &vcpu_data->guest_vmcb.save_state_area.lbr_stack[0];

    //if (BranchTracer::active)
    //{
    //    vcpu_data->guest_vmcb.save_state_area.DBGEXTNCFG |= (1 << 6);
    //    
    //    if (vcpu_data->guest_vmcb.control_area.NCr3 != Hypervisor::Get()->ncr3_dirs[sandbox_single_step])
    //    {
    //        for (int i = 0; i < BranchTracer::lbr_stack_size; ++i)
    //        {
    //            if (lbr_stack[i].address == BranchTracer::cur_basic_block.end)
    //            {
    //                BranchTracer::LogToUsermode(lbr_stack[i]);

    //                BranchTracer::log_buffer.cur_block.start = lbr_stack[i].target;

    //                BranchTracer::cur_basic_block.end = Disasm::ForEachInstruction(BranchTracer::cur_basic_block.start, BranchTracer::cur_basic_block.start + PAGE_SIZE, 
    //                    [](uint8_t* insn_addr, ZydisDecodedInstruction insn) -> bool {
    //                        if (instruction.meta.category == ZydisInstructionCategory::ZYDIS_CATEGORY_COND_BR ||
    //                            instruction.meta.category == ZydisInstructionCategory::ZYDIS_CATEGORY_UNCOND_BR ||
    //                            instruction.meta.category == ZydisInstructionCategory::ZYDIS_CATEGORY_CALL)
    //                        {
    //                            return false;
    //                        }   
    //                    }
    //                );
    //            }
    //        }
    //    }
    //}

    DR6 dr6;

    dr6.Flags = vcpu_data->guest_vmcb.save_state_area.Dr6;

    if (dr6.SingleInstruction == 1) 
    {
        RFLAGS rflag;
        rflag.Flags = vcpu_data->guest_vmcb.save_state_area.Rflags, 
        rflag.TrapFlag = 0;
       
        vcpu_data->guest_vmcb.save_state_area.Rflags = rflag.Flags;

        /*	single-step the read/write in the ncr3 that allows all pages to be executable	*/

        if (vcpu_data->guest_vmcb.control_area.NCr3 == Hypervisor::Get()->ncr3_dirs[sandbox_single_step])
        {
            DbgPrint("Finished single stepping %p \n", vcpu_data->guest_vmcb.save_state_area.Rip);

            vcpu_data->guest_vmcb.control_area.NCr3 = Hypervisor::Get()->ncr3_dirs[primary];

            vcpu_data->guest_vmcb.save_state_area.Rsp -= 8;

            *(uintptr_t*)vcpu_data->guest_vmcb.save_state_area.Rsp = guest_rip;

            vcpu_data->guest_vmcb.save_state_area.Rip = (uintptr_t)Sandbox::sandbox_hooks[Sandbox::readwrite_handler];

            vcpu_data->guest_vmcb.control_area.VmcbClean &= 0xFFFFFFEF;
            vcpu_data->guest_vmcb.control_area.TlbControl = 1;
        }
    }
    else      
    {
        InjectException(vcpu_data, EXCEPTION_VECTOR::Debug, FALSE, 0);
    }

    __writecr3(vmroot_cr3);
}
