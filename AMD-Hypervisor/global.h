#pragma once
#include "AMD_Define.h"

#define DIFFERENCE(a, b)    max(a,b) - min(a, b)


struct NPTHOOK_ENTRY;

struct HYPERVISOR_DATA
{
    ULONG64     PrimaryNCr3;
    ULONG64     SecondaryNCr3;
    ULONG64     TertiaryCr3;
    NPTHOOK_ENTRY*  FirstHook;
    LIST_ENTRY*  HookListHead;
    ULONG   ImageSize;
    PVOID   ImageStart;
    HANDLE  TargetPID;
    UINT64  HvCommand;
};

extern HYPERVISOR_DATA* g_HvData;

/*	
	one for each core.

	StackSpace - Stack Space required because we are manually setting stack pointer to GuestVmcbPa
	We need to also subtract some size to make VMCB 4KB aligned	& GuestVmcbPa 16 byte aligned
		
	Self - Pointer to vprocessor on stack
*/
struct VPROCESSOR_DATA
{
	char        StackSpace[KERNEL_STACK_SIZE - sizeof(PVOID) * 4];	
	ULONG64	    GuestVmcbPa;	// <------ stack pointer points here
	ULONG64	    HostVmcbPa;
	PVOID       Self;			
    ULONG64     Pad;
    VMCB        GuestVmcb;
	VMCB        HostVmcb;
    char        HostSaveArea[0x1000];
};
extern int  CoreCount;
extern VPROCESSOR_DATA*    g_VpData[32];


struct registers
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


