#pragma once
#include "includes.h"


/* VMCB exception intercept vector 2    */

union InterceptVector2
{
    struct
    {
        int32_t intercept_de : 1;
        int32_t intercept_db : 1;
        int32_t intercept_nmi : 1;
        int32_t intercept_bp : 1;
        int32_t intercept_of : 1;
        int32_t intercept_br : 1;
        int32_t intercept_ud : 1;
        int32_t intercept_nm : 1;
        int32_t intercept_df : 1;
        int32_t reserved1 : 1;
        int32_t intercept_ts : 1;
        int32_t intercept_np : 1;
        int32_t intercept_ss : 1;
        int32_t intercept_gp : 1;
        int32_t intercept_pf : 1;
        int32_t pad : 15;
        int32_t intercept_db2 : 1;
        int32_t pad17 : 1;
    };
    int32_t as_int32;
};
static_assert(sizeof(InterceptVector2) == 0x4, "InterceptVector2 Size Mismatch");

/* VMCB exception intercept vector 4    */

union InterceptVector4
{
    struct
    {
        int32_t intercept_vmrun : 1;
        int32_t intercept_vmmcall : 1;
        int32_t Intercept_vmload : 1;
        int32_t intercept_vmsave : 1;
        int32_t Intercept_stgi : 1;
        int32_t Intercept_clgi : 1;
        int32_t Intercept_skinit : 1;
        int32_t intercept_RDTSCP : 1;
        int32_t intercept_icebp : 1;
        int32_t Intercept_wbinvd : 1;
        int32_t Intercept_monitor : 1;
        int32_t intercept_mwait : 1;
        int32_t intercept_mwait2 : 1;
        int32_t Intercept_xsetbv : 1;
        int32_t Intercept_rdpru : 1;
        int32_t intercept_efer : 1;
        int32_t write_cr : 16;
    };
    int32_t	as_int32;
};
static_assert(sizeof(InterceptVector4) == 0x4, "InterceptVector4 Size Mismatch");

/*  #NPF information, saved in EXITINFO1    */

struct NestedPageFaultInfo1
{
    union
    {
        uint64_t as_uint64;
        struct
        {
            uint64_t valid : 1;                   // [0]
            uint64_t write : 1;                   // [1]
            uint64_t user : 1;                    // [2]
            uint64_t reserved : 1;                // [3]
            uint64_t execute : 1;                 // [4]
            uint64_t reserved2 : 27;              // [5:31]
            uint64_t guestphysicaladdress : 1;    // [32]
            uint64_t guestpagetables : 1;         // [33]
        } fields;
    };
};

/*  Thank you to Satoshi Tanda @tandasat for writing out the VMCB structures  */

#pragma pack(1)
struct VmcbControlArea
{
    UINT16 intercept_cr_read;           // +0x000
    UINT16 InterceptCrWrite;            // +0x002
    UINT16 intercept_dr_read;           // +0x004
    UINT16 intercept_dr_write;          // +0x006
    UINT32 intercept_exception;         // +0x008
    UINT32 intercept_vec3;              // +0x00c
    UINT32 intercept_vec4;              // +0x010
    UINT8 reserved1[0x03c - 0x014];     // +0x014 there should be intercept vector 5 here
    UINT16 pause_filter_threshold;      // +0x03c
    UINT16 pause_filter_count;          // +0x03e
    UINT64 iopm_base_pa;            // +0x040
    UINT64 msrpm_base_pa;           // +0x048
    UINT64 tsc_offset;              // +0x050
    UINT32 guest_asid;              // +0x058
    UINT32 tlb_control;             // +0x05c
    UINT64 v_intr;                  // +0x060
    UINT64 interrupt_shadow;        // +0x068
    UINT64 exit_code;               // +0x070
    UINT64 exit_info1;              // +0x078
    UINT64 exit_info2;              // +0x080
    UINT64 exit_int_info;           // +0x088
    UINT64 np_enable;               // +0x090
    UINT64 avic_apic_bar;           // +0x098
    UINT64 guest_pa_of_ghcb;        // +0x0a0
    UINT64 event_inject;                // +0x0a8
    UINT64 ncr3;                        // +0x0b0
    UINT64 lbr_virtualization_enable;   // +0x0b8
    UINT64 vmcb_clean;                  // +0x0c0
    UINT64 nrip;                        // +0x0c8
    UINT8 num_of_bytes_fetched;         // +0x0d0
    UINT8 guest_instruction_bytes[15];  // +0x0d1
    UINT64 avic_apic_backing_page_ptr;  // +0x0e0
    UINT64 reserved2;                   // +0x0e8
    UINT64 avic_logical_table_ptr;      // +0x0f0
    UINT64 avic_physical_table_ptr;     // +0x0f8
    UINT64 reserved3;                   // +0x100
    UINT64 vmcb_save_state_ptr;         // +0x108
    UINT8 reserved4[0x400 - 0x110];     // +0x110
};
#pragma pack()

static_assert(sizeof(VmcbControlArea) == 0x400, "Vmcbcontrol_area Size Mismatch");

//
// See "VMCB Layout, State Save Area"
//
typedef struct VmcbSaveStateArea
{
    UINT16 es_selector;     // +0x000
    UINT16 es_attrib;       // +0x002
    UINT32 es_limit;        // +0x004
    UINT64 es_base;         // +0x008
    UINT16 cs_selector;     // +0x010
    UINT16 cs_attrib;       // +0x012
    UINT32 cs_limit;        // +0x014
    UINT64 cs_base;         // +0x018
    UINT16 ss_selector;     // +0x020
    UINT16 ss_attrib;       // +0x022
    UINT32 ss_limit;        // +0x024
    UINT64 ss_base;         // +0x028
    UINT16 ds_selector;     // +0x030
    UINT16 ds_attrib;       // +0x032
    UINT32 ds_limit;        // +0x034
    UINT64 ds_base;         // +0x038
    UINT16 fs_selector;     // +0x040
    UINT16 fs_attrib;       // +0x042
    UINT32 fs_limit;        // +0x044
    UINT64 fs_base;         // +0x048
    UINT16 gs_selector;     // +0x050
    UINT16 gs_attrib;       // +0x052
    UINT32 gs_limit;        // +0x054
    UINT64 gs_base;         // +0x058
    UINT16 gdtr_selector;   // +0x060
    UINT16 gdtr_attrib;     // +0x062
    UINT32 gdtr_limit;      // +0x064
    UINT64 gdtr_base;       // +0x068
    UINT16 ldtr_selector;   // +0x070
    UINT16 ldtr_attrib;     // +0x072
    UINT32 ldtr_limit;      // +0x074
    UINT64 ldtr_base;       // +0x078
    UINT16 idtr_selector;   // +0x080
    UINT16 idtr_attrib;     // +0x082
    UINT32 idtr_limit;      // +0x084
    UINT64 idtr_base;       // +0x088
    UINT16 tr_selector;     // +0x090
    UINT16 tr_attrib;       // +0x092
    UINT32 tr_limit;        // +0x094
    UINT64 tr_base;         // +0x098
    UINT8 reserved1[0x0cb - 0x0a0];     // +0x0a0
    UINT8 cpl;                          // +0x0cb
    UINT32 reserved2;                   // +0x0cc
    UINT64 efer;                        // +0x0d0
    UINT8 reserved3[0x148 - 0x0d8];     // +0x0d8
    CR4 cr4;                            // +0x148
    CR3 cr3;                            // +0x150
    CR0 cr0;                            // +0x158
    DR7 dr7;                            // +0x160
    DR6 dr6;                            // +0x168
    RFLAGS rflags;                      // +0x170
    UINT64 rip;                         // +0x178
    UINT8 reserved4[0x1d8 - 0x180];     // +0x180
    UINT64 rsp;                         // +0x1d8
    UINT8 reserved5[0x1f8 - 0x1e0];     // +0x1e0
    UINT64 rax;                         // +0x1f8
    UINT64 star;                        // +0x200
    UINT64 lstar;                       // +0x208
    UINT64 cstar;                       // +0x210
    UINT64 sfmask;                      // +0x218
    UINT64 kernel_gs_base;              // +0x220
    UINT64 sysenter_cs;                 // +0x228
    UINT64 sysenter_esp;                // +0x230
    UINT64 sysenter_eip;                // +0x238
    UINT64 cr2;                         // +0x240
    UINT8 reserved6[0x268 - 0x248];     // +0x248
    UINT64 guest_pat;                   // +0x268
    IA32_DEBUGCTL_REGISTER dbg_ctl;     // +0x270
    UINT64 br_from;                     // +0x278
    UINT64 br_to;                       // +0x280
    UINT64 last_excep_from;             // +0x288
    UINT64 last_excep_to;               // +0x290 Why isnt this in VMCB layout table?
    UINT64 guest_debug_extnctl;         // +0x298

    UINT8 Reserved7[0x670 - 0x2A0];     // +0x2A0

    struct GuestLBRStack
    {
        UINT64 address;
        UINT64 target;
    } lbr_stack[16];                    // +0x670

    UINT64 LBR_SELECT;                  // +0x770
};

/*	#include <pshpack1.h> to remove struct alignment, so we wont have GDTR value issues	
*   d
*/

#include <pshpack1.h>
struct DescriptorTableRegister
{
    uint16_t limit;
    uintptr_t base;
};

static_assert(sizeof(DescriptorTableRegister) == 0xA, "DESCRIPTOR_TABLE_REGISTER Size Mismatch");

/*  15.20 Event Injection   */
union EventInjection
{
    struct
    {
        int     vector : 8;
        int     type : 3;
        int     push_error_code : 1;
        int     reserved : 19;
        int     valid : 1;
        int     error_code : 32;
    };
    int64_t fields;
};


/*  must be 4KB aligned     */
struct VMCB
{
    VmcbControlArea control_area;
    VmcbSaveStateArea save_state_area;
    char pad[PAGE_SIZE - sizeof(VmcbControlArea) - sizeof(VmcbSaveStateArea)];
};
