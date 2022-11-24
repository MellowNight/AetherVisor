#pragma once
#include "amd_definitions.h"

/*
    CoreVmcbData:
    A structure which contains some core-specific data, must be 16 byte aligned on the stack

    StackSpace - Stack Space required because we are manually setting stack pointer to guest_vmcbPa
    We need to also subtract some size to make VMCB 4KB aligned	& guest_vmcbPa 16 byte aligned

    Self - Pointer to vprocessor on stack
*/

struct VcpuData
{
    uint8_t     stack_space[KERNEL_STACK_SIZE - sizeof(uint64_t) * 4];
    uintptr_t   guest_vmcb_physicaladdr;	// <------ stack pointer points here
    uintptr_t   host_vmcb_physicaladdr;
    struct VcpuData* self;
    uint8_t     pad[8];
    VMCB        guest_vmcb;
    VMCB        host_vmcb;
    uint8_t     host_save_area[0x1000];
};

/* the hypervisor    */

class Hypervisor
{
private:
    static Hypervisor* hypervisor;

    /*  i dont feel like overloading the new operator, so init() will be constructor */

    void Init()
    {
        core_count = KeQueryActiveProcessorCount(0);

        for (int i = 0; i < 32; ++i)
        {
            vcpu_data[i] = NULL;
        }
    }
public:

    uintptr_t ncr3_dirs[3];

    PHYSICAL_MEMORY_RANGE phys_mem_range[12];
    VcpuData* vcpu_data[32];
    int core_count;

    /* Static access method. */
    static Hypervisor* Get();

    static char disk_serial[13];

public:
    bool IsHypervisorPresent(
        int32_t core_number
    );
};