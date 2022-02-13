#pragma once
#include "amd_definitions.h"

#define DIFFERENCE(a, b)    max(a,b) - min(a, b)

struct HYPERVISOR_DATA
{
    uintptr_t     PrimaryNCr3;
    uintptr_t     SecondaryNCr3;
    uintptr_t     TertiaryCr3;
    struct NptHookEntry*  FirstHook;
    LIST_ENTRY*     HookListHead;
};

extern HYPERVISOR_DATA* g_HvData;

/*	
*   CoreVmcbData: 
	A structure which contains some core-specific data, must be 16 byte aligned on the stack

	StackSpace - Stack Space required because we are manually setting stack pointer to GuestVmcbPa
	We need to also subtract some size to make VMCB 4KB aligned	& GuestVmcbPa 16 byte aligned
		
	Self - Pointer to vprocessor on stack
*/
struct CoreVmcbData
{
	char        StackSpace[KERNEL_STACK_SIZE - sizeof(void*) * 4];	
	uintptr_t   guest_vmcb_physicaladdr;	// <------ stack pointer points here
	uintptr_t   host_vmcb_physicaladdr;
	void*       Self;			
    char        pad[8];
    VMCB        GuestVmcb;
	VMCB        HostVmcb;
    char        HostSaveArea[0x1000];
};
extern int  CoreCount;
extern CoreVmcbData*    g_VpData[32];


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


