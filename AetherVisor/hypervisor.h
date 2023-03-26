#pragma once
#include "svm.h"

struct GuestRegisters
{
    uintptr_t  r15;
    uintptr_t  r14;
    uintptr_t  r13;
    uintptr_t  r12;
    uintptr_t  r11;
    uintptr_t  r10;
    uintptr_t  r9;
    uintptr_t  r8;
    uintptr_t  rdi;
    uintptr_t  rsi;
    uintptr_t  rbp;
    uintptr_t  rsp;
    uintptr_t  rbx;
    uintptr_t  rdx;
    uintptr_t  rcx;
    uintptr_t  rax;

    uintptr_t* operator[] (int32_t gpr_number)
    {
        return &((uintptr_t*)this)[15 - gpr_number];
    }
};

/*
*   VcpuData:
*   Contains core-specific VMCB data and other information. Must be 16 byte aligned on the stack
*
*   StackSpace - Stack Space required because we are manually setting stack pointer to guest_vmcbPa
*   We need to also subtract some size to make VMCB 4KB aligned	& guest_vmcbPa 16 byte aligned
*
*   Self - VcpuData self-reference
*/

struct VcpuData
{
    uint8_t     stack_space[KERNEL_STACK_SIZE - sizeof(int64_t) * 4];
    uintptr_t   guest_vmcb_physicaladdr;	// <------ stack pointer points here
    uintptr_t   host_vmcb_physicaladdr;
    struct      VcpuData* self;
    uintptr_t   suppress_nrip_increment;
    VMCB        guest_vmcb;
    VMCB        host_vmcb;
    uint8_t     host_save_area[0x1000];

    void ConfigureProcessor(CONTEXT* context_record);

    bool IsPagePresent(void* address);

    void InjectException(
        int vector, 
        bool push_error, 
        int error_code
    );

    void VmmcallHandler(
        GuestRegisters* guest_regs,
        bool* end_svm
    );

    void BreakpointHandler(
        GuestRegisters* guest_ctx
    );

    void DebugFaultHandler(
        GuestRegisters* guest_ctx
    );

    bool InvalidOpcodeHandler(
        GuestRegisters* guest_ctx
    );

    void MsrExitHandler(
        GuestRegisters* guest_regs
    );

    void NestedPageFaultHandler(
        GuestRegisters* guest_registers
    );

    void DebugRegisterExit(GuestRegisters* guest_ctx);
    void PushfExit(GuestRegisters* guest_ctx);
    void PopfExit(GuestRegisters* guest_ctx);
};

/* Global hypervisor information    */

class Hypervisor
{
private:
    static Hypervisor* hypervisor;

    void Init()
    {
        core_count = KeQueryActiveProcessorCount(0);

        for (int i = 0; i < 32; ++i)
        {
            vcpus[i] = NULL;
        }
    }
public:

    uintptr_t ncr3_dirs[4];

    PHYSICAL_MEMORY_RANGE phys_mem_range[12];

    VcpuData* vcpus[32];

    int core_count;

    static Hypervisor* Get();

    bool IsCoreVirtualized(int32_t core_number);
    void CleanupOnProcessExit();
};