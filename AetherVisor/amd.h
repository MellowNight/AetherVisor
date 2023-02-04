#pragma once
#include "includes.h"

/*  enum for cpuid codes    */

enum CPUID
{
    vendor_and_max_standard_fn_number = 0x0,
    feature_identifier = 0x80000001,
    ext_perfmon_and_debug = 0x80000022,
    svm_features = 0x8000000A,
};

/*  enum for some model specific register numbers    */

enum MSR : uint64_t
{
    pat = 0x277,        /*  Page Atrribute Table  */
    apic_bar = 0x1b,
    vm_cr = 0xC0010114,
    efer = 0xC0000080,
    vm_hsave_pa = 0xC0010117
};


/*	AMD EFER (Extended Feature Enable Register) MSR     */

struct EferMsr
{
    union
    {
        __int64	flags;
        struct
        {
            uint32_t    syscall : 1;
            uint32_t    reserved : 7;
            uint32_t    long_mode_enable : 1;
            uint32_t    reserved2 : 1;
            uint32_t    long_mode_active : 1;
            uint32_t    nxe : 1;
            uint32_t    svme : 1;
            uint32_t    lmsle : 1;
            uint32_t    ffxse : 1;
            uint32_t    tce : 1;
            uint32_t    reserved3 : 1;
            uint32_t    m_commit : 1;
            uint32_t    intwb : 1;
            __int64     reserved4 : 45;
        };
    };
};

/*	 Advanced Programmable Interrupt Controller Base Address Register MSR	*/

struct ApicBarMsr
{
    union
    {
        uint64_t flags;
        struct
        {
            uint64_t reserved1 : 8;             // [0:7]
            uint64_t bootstrap_processor : 1;   // [8]
            uint64_t reserved2 : 1;             // [9]
            uint64_t enable_x2apic_mode : 1;    // [10]
            uint64_t enable_xapic_global : 1;   // [11]
            uint64_t apic_base : 24;            // [12:35]
        };
    };
};

/* Struct to store x86 long mode segment descriptor fields.  */

struct SegmentDescriptor
{
    union
    {
        uint64_t as_int64;
        struct
        {
            uint16_t LimitLow;        // [0:15]
            uint16_t BaseLow;         // [16:31]
            uint32_t BaseMiddle : 8;  // [32:39]
            uint32_t Type : 4;        // [40:43]
            uint32_t System : 1;      // [44]
            uint32_t Dpl : 2;         // [45:46]
            uint32_t Present : 1;     // [47]
            uint32_t LimitHigh : 4;   // [48:51]
            uint32_t Avl : 1;         // [52]
            uint32_t LongMode : 1;    // [53]
            uint32_t DefaultBit : 1;  // [54]
            uint32_t Granularity : 1; // [55]
            uint32_t BaseHigh : 8;    // [56:63]
        };
    };
};
static_assert(sizeof(SegmentDescriptor) == 8, "SegmentDescriptor Size Mismatch");

/* Struct to store the segment descriptor fields used by the VMCB.  */

struct SegmentAttribute
{
    union
    {
        uint16_t as_uint16;
        struct
        {
            uint16_t type : 4;        // [0:3]
            uint16_t system : 1;      // [4]
            uint16_t dpl : 2;         // [5:6]
            uint16_t present : 1;     // [7]
            uint16_t avl : 1;         // [8]
            uint16_t long_mode : 1;   // [9]
            uint16_t default_bit : 1; // [10]
            uint16_t granularity : 1; // [11]
            uint16_t reserved1 : 4;   // [12:15]
        } fields;
    };
};


/*	 Core::X86::Msr::VM_CR	*/

union MsrVmcr
{
    struct
    {
        int		reserved1 : 1;
        int		intercept_init : 1;
        int		reserved2 : 1;
        int		svm_lock : 1;
        int		svme_disable : 1;
        int		reserved3 : 27;
        int		reserved4 : 32;
    };
    int64_t	flags;
};


union AddressTranslationHelper
{
    // Indexes to locate paging-structure entries corresponds to this virtual
    // address.
    //
    struct
    {
        uint64_t page_offset : 12;  //< [11:0]
        uint64_t pt : 9;            //< [20:12]
        uint64_t pd : 9;            //< [29:21]
        uint64_t pdpt : 9;          //< [38:30]
        uint64_t pml4 : 9;          //< [47:39]
    } AsIndex;

    uint64_t as_int64;
};

extern "C" void _sgdt(void* Descriptor);

extern "C" int16_t __readtr();


