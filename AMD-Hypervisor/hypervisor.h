#pragma once
#include "amd_definitions.h"

/*
    CoreVmcbData:
    A structure which contains some core-specific data, must be 16 byte aligned on the stack

    StackSpace - Stack Space required because we are manually setting stack pointer to guest_vmcbPa
    We need to also subtract some size to make VMCB 4KB aligned	& guest_vmcbPa 16 byte aligned

    Self - Pointer to vprocessor on stack
*/

struct CoreVmcbData
{
    uint8_t     stack_space[KERNEL_STACK_SIZE - sizeof(uint64_t) * 4];
    uintptr_t   guest_vmcb_physicaladdr;	// <------ stack pointer points here
    uintptr_t   host_vmcb_physicaladdr;
    struct CoreVmcbData* self;
    uint8_t     pad[8];
    Vmcb        guest_vmcb;
    Vmcb        host_vmcb;
    uint8_t     host_save_area[0x1000];
};

struct Hypervisor
{
    uintptr_t normal_ncr3;
    uintptr_t noexecute_ncr3;

    PHYSICAL_MEMORY_RANGE phys_mem_range[12]; 
    CoreVmcbData* vcpu_data[32];
    int core_count;


    bool IsHypervisorPresent(
        int32_t core_number
    );
};

extern Hypervisor* hypervisor;

