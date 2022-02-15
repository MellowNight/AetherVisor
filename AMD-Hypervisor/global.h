#pragma once
#include "amd_definitions.h"

struct GlobalHvData
{
    uintptr_t     normal_ncr3;
    uintptr_t     noexecute_ncr3;
    uintptr_t     tertiary_cr3;
    struct NptHookEntry*  first_hook;
    LIST_ENTRY*     hook_list_head;
};

extern GlobalHvData* global_hypervisor_data;

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
	void*       self;			
    uint8_t     pad[8];
    Vmcb        guest_vmcb;
    Vmcb        host_vmcb;
    uint8_t     host_save_state_area[0x1000];
};
extern int  core_count;
extern CoreVmcbData*    hv_core_data[32];


struct GeneralPurposeRegs
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


