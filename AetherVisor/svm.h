#pragma once
#include "amd.h"

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


/*  VMCB intercept vector 2, used for exceptions */

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



/*  the VMCB structures are typed out by Satoshi Tanda @tandasat    */

struct VmcbControlArea
{
    uint16_t intercept_cr_read;             // +0x000
    uint16_t intercept_cr_write;            // +0x002
    uint16_t intercept_dr_read;             // +0x004
    uint16_t intercept_dr_write;            // +0x006
    InterceptVector2 intercept_exception;           // +0x008
    uint32_t intercept_vec3;                // +0x00c
    InterceptVector4 intercept_vec4;                // +0x010
    uint8_t reserved1[0x03c - 0x014];       // +0x014 there should be intercept vector 5 here
    uint16_t pause_filter_threshold;        // +0x03c
    uint16_t pause_filter_count;            // +0x03e
    uint64_t iopm_base_pa;                  // +0x040
    uint64_t msrpm_base_pa;                 // +0x048
    uint64_t tsc_offset;                    // +0x050
    uint32_t guest_asid;                    // +0x058
    uint32_t tlb_control;                   // +0x05c
    uint64_t v_intr;                        // +0x060
    uint64_t interrupt_shadow;              // +0x068
    uint64_t exit_code;                     // +0x070
    uint64_t exit_info1;                    // +0x078
    uint64_t exit_info2;                    // +0x080
    uint64_t exit_int_info;                 // +0x088
    uint64_t np_enable;                     // +0x090
    uint64_t avic_apic_bar;                 // +0x098
    uint64_t ghcb_guest_pa;                 // +0x0a0
    uint64_t event_inject;                  // +0x0a8
    uint64_t ncr3;                          // +0x0b0
    uint64_t lbr_virt_enable;               // +0x0b8
    uint64_t vmcb_clean;                    // +0x0c0
    uint64_t nrip;                          // +0x0c8
    uint8_t bytes_fetched_count;            // +0x0d0
    uint8_t guest_instruction_bytes[15];    // +0x0d1
    uint64_t avic_apic_backing_page_ptr;    // +0x0e0
    uint64_t reserved2;                     // +0x0e8
    uint64_t avic_logical_table_ptr;        // +0x0f0
    uint64_t avic_physical_table_ptr;       // +0x0f8
    uint64_t reserved3;                     // +0x100
    uint64_t vmcb_save_state_ptr;           // +0x108
    uint8_t reserved4[0x400 - 0x110];       // +0x110
};
static_assert(sizeof(VmcbControlArea) == 0x400, "VmcbControlArea Size Mismatch");

//
// See "VMCB Layout, State Save Area"
//
typedef struct VmcbSaveStateArea
{
    uint16_t es_selector;                  // +0x000
    SegmentAttribute es_attrib;                    // +0x002
    uint32_t es_limit;                     // +0x004
    uint64_t es_base;                      // +0x008
    uint16_t cs_selector;                  // +0x010
    SegmentAttribute cs_attrib;                    // +0x012
    uint32_t cs_limit;                     // +0x014
    uint64_t cs_base;                      // +0x018
    uint16_t ss_selector;                  // +0x020
    SegmentAttribute ss_attrib;                    // +0x022
    uint32_t ss_limit;                     // +0x024
    uint64_t ss_base;                      // +0x028
    uint16_t ds_selector;                  // +0x030
    SegmentAttribute ds_attrib;                    // +0x032
    uint32_t ds_limit;                     // +0x034
    uint64_t ds_base;                      // +0x038
    uint16_t fs_selector;                  // +0x040
    uint16_t fs_attrib;                    // +0x042
    uint32_t fs_limit;                     // +0x044
    uint64_t fs_base;                      // +0x048
    uint16_t gs_selector;                  // +0x050
    uint16_t gs_attrib;                    // +0x052
    uint32_t gs_limit;                     // +0x054
    uint64_t gs_base;                      // +0x058
    uint16_t gdtr_selector;                // +0x060
    uint16_t gdtr_attrib;                  // +0x062
    uint32_t gdtr_limit;                   // +0x064
    uint64_t gdtr_base;                    // +0x068
    uint16_t ldtr_selector;                // +0x070
    uint16_t ldtr_attrib;                  // +0x072
    uint32_t ldtr_limit;                   // +0x074
    uint64_t ldtr_base;                    // +0x078
    uint16_t idtr_selector;                // +0x080
    uint16_t idtr_attrib;                  // +0x082
    uint32_t idtr_limit;                   // +0x084
    uint64_t idtr_base;                    // +0x088
    uint16_t tr_selector;                  // +0x090
    uint16_t tr_attrib;                    // +0x092
    uint32_t tr_limit;                     // +0x094
    uint64_t trbase;                      // +0x098
    uint8_t reserved1[0x0cb - 0x0a0];     // +0x0a0
    uint8_t cpl;                          // +0x0cb
    uint32_t reserved2;                   // +0x0cc
    EFER_MSR efer;                        // +0x0d0
    uint8_t reserved3[0x148 - 0x0d8];     // +0x0d8
    CR4 cr4;                         // +0x148
    CR3 cr3;                         // +0x150
    CR0 cr0;                         // +0x158
    DR7 dr7;                         // +0x160
    DR6 dr6;                         // +0x168
    RFLAGS rflags;                      // +0x170
    uint64_t rip;                         // +0x178
    uint8_t Reserved4[0x1d8 - 0x180];     // +0x180
    uint64_t rsp;                         // +0x1d8
    uint8_t Reserved5[0x1f8 - 0x1e0];     // +0x1e0
    uint64_t rax;                         // +0x1f8
    uint64_t star;                        // +0x200
    uint64_t lstar;                       // +0x208
    uint64_t cstar;                       // +0x210
    uint64_t sf_mask;                      // +0x218
    uint64_t kernel_gs_base;                // +0x220
    uint64_t sysenter_cs;                  // +0x228
    uint64_t sysenter_esp;                 // +0x230
    uint64_t sysenter_eip;                 // +0x238
    uint64_t cr2;                       // +0x240
    uint8_t reserved6[0x268 - 0x248];     // +0x248
    uint64_t guest_pat;                        // +0x268
    IA32_DEBUGCTL_REGISTER  dbg_ctl;    // +0x270
    uint64_t br_from;                      // +0x278
    uint64_t br_to;                        // +0x280
    uint64_t last_excep_from;               // +0x288
    uint64_t last_excep_to;                 // +0x290
    uint8_t reserved7[0x7C8 - 0x298];
};
static_assert(sizeof(VmcbSaveStateArea) == 0x7C8, "VmcbSaveStateArea Size Mismatch");

/*  must be 4KB aligned     */

struct VMCB
{
    VmcbControlArea control_area;
    VmcbSaveStateArea save_state_area;
    char pad[PAGE_SIZE - sizeof(VmcbControlArea) - sizeof(VmcbSaveStateArea)];
};

/*  EXITINFO1 for mov CRx and mov DRx  */

#define SVM_EXITINFO_REG_MASK 0x0F

#define SVM_INTERCEPT_POPF 17
#define SVM_INTERCEPT_PUSHF 16

#define SVM_NP_ENABLE 0
