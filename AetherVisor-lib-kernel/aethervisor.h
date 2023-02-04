#pragma once
#include "vmmcall.h"
#include "utils.h"

#define PAGE_SIZE 0x1000

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
};


extern "C" {
    
    void (*sandbox_execute_handler)(GuestRegisters* registers, void* return_address, void* o_guest_rip);
    void __stdcall execute_handler_wrap();

    void (*sandbox_mem_access_handler)(GuestRegisters * registers, void* o_guest_rip);
    void __stdcall rw_handler_wrap();
    
    void (*branch_log_full_handler)();
    void __stdcall branch_log_full_handler_wrap();

    void (*branch_trace_finish_handler)();
    void __stdcall branch_trace_finish_handler_wrap();

    int __stdcall svm_vmmcall(VMMCALL_ID vmmcall_id, ...);
}

namespace AetherVisor
{
    enum NCR3_DIRECTORIES
    {
        primary,
        shadow,
        sandbox,
        sandbox_single_step
    };

    enum CALLBACK_ID
    {
        sandbox_readwrite = 0,
        sandbox_execute = 1,
        branch_log_full = 2,
        branch_trace_finished = 3,
        max_id
    };

    void TraceFunction(
        uint8_t* start_addr, 
        uintptr_t range_base, 
        uintptr_t range_size
    );

    int SandboxPage(
        uintptr_t address, 
        uintptr_t tag
    );

    void SandboxRegion(
        uintptr_t base, 
        uintptr_t size
    );

    void DenySandboxMemAccess(
        void* page_addr
    );

    void SetCallback(
        CALLBACK_ID handler_id, 
        void* address
    );


    int StopHv();
};