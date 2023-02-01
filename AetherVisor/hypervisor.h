#pragma once
#include "svm.h"

struct GuestRegisters
{
    UINT64  r15;
    UINT64  r14;
    UINT64  r13;
    UINT64  r12;
    UINT64  r11;
    UINT64  r10;
    UINT64  r9;
    UINT64  r8;
    UINT64  rdi;
    UINT64  rsi;
    UINT64  rbp;
    UINT64  rsp;
    UINT64  rbx;
    UINT64  rdx;
    UINT64  rcx;
    UINT64  rax;
};

/*
    VcpuData:
    A structure which contains some core-specific data, must be 16 byte aligned on the stack

    StackSpace - Stack Space required because we are manually setting stack pointer to guest_vmcbPa
    We need to also subtract some size to make VMCB 4KB aligned	& guest_vmcbPa 16 byte aligned

    Self - VcpuData self-reference 
*/

struct VcpuData
{
    uint8_t     stack_space[KERNEL_STACK_SIZE - sizeof(int64_t) * 4];
    uintptr_t   guest_vmcb_physicaladdr;	// <------ stack pointer points here
    uintptr_t   host_vmcb_physicaladdr;
    struct      VcpuData* self;
    uint8_t     pad[8];
    VMCB        guest_vmcb;
    VMCB        host_vmcb;
    uint8_t     host_save_area[0x1000];
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
            vcpu_data[i] = NULL;
        }
    }
public:

    uintptr_t ncr3_dirs[4];

    PHYSICAL_MEMORY_RANGE phys_mem_range[12];

    VcpuData* vcpu_data[32];

    int core_count;

    static Hypervisor* Get();

public:

    bool IsCoreVirtualized(int32_t core_number);
};