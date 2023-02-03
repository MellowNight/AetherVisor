#pragma once
#include "svm.h"
#include "paging_utils.h"

struct GuestRegs
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
};

/*
    VcpuData:
    Contains core-specific VMCB data and other information. Must be 16 byte aligned on the stack

    StackSpace - Stack Space required because we are manually setting stack pointer to guest_vmcbPa
    We need to also subtract some size to make VMCB 4KB aligned	& guest_vmcbPa 16 byte aligned

    Self - VcpuData self-reference 
*/

struct VcpuData
{
    uint8_t     stack_space[KERNEL_STACK_SIZE - sizeof(int64_t) * 5];
    uintptr_t   guest_vmcb_physicaladdr;	// <------ stack pointer points here
    uintptr_t   host_vmcb_physicaladdr;
    PhysMemAccess* mem_access;
    struct      VcpuData* self;
    uint8_t     pad[8];
    VMCB        guest_vmcb;
    VMCB        host_vmcb;
    uint8_t     host_save_area[0x1000];

    void InjectException(int vector, bool push_error, int error_code);

    void VmmcallHandler(
        GuestRegs* GuestRegs,
        bool* EndVM
    );

    void BreakpointHandler(
        GuestRegs* guest_ctx
    );

    void DebugFaultHandler(
        GuestRegs* guest_ctx
    );

    bool InvalidOpcodeHandler(
        GuestRegs* guest_ctx,
        PhysMemAccess* physical_mem
    );

    void MsrExitHandler(
        GuestRegs* guest_regs
    );

    void NestedPageFaultHandler(
        GuestRegs* guest_registers
    );

    void ConfigureProcessor(CONTEXT* context_record);
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
            vcpu[i] = NULL;
        }
    }
public:

    uintptr_t ncr3_dirs[4];

    PHYSICAL_MEMORY_RANGE phys_mem_range[12];

    VcpuData* vcpu[32];

    int core_count;

    static Hypervisor* Get();

    bool IsCoreVirtualized(int32_t core_number);
};