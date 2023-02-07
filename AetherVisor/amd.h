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

struct SegmentDescriptor
{
    union
    {
        uint64_t value;
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


/*	Core::X86::MSR::efer	*/

union EFER_MSR
{
    struct 
    {
        uint64_t syscall : 1;
        uint64_t reserved1 : 7;
        uint64_t long_mode_enable : 1;
        uint64_t reserved2 : 1;
        uint64_t long_mode_active : 1;
        uint64_t nx_page : 1;
        uint64_t svme : 1;
        uint64_t lmsle : 1;
        uint64_t ffxse : 1;
        uint64_t reserved3 : 1;
        uint64_t reserved4 : 47;
    };
    uint64_t value;
};

static_assert(sizeof(EFER_MSR) == 8, "EFER MSR Size Mismatch");


/*	 Core::X86::Msr::APIC_BAR	*/
struct APIC_BAR_MSR
{
    union
    {
        uint64_t value;
        struct
        {
            uint64_t reserved1 : 8;           // [0:7]
            uint64_t bootstrap_processor : 1;  // [8]
            uint64_t reserved2 : 1;           // [9]
            uint64_t x2apic_mode : 1;    // [10]
            uint64_t xapic_global : 1;   // [11]
            uint64_t apic_base : 24;           // [12:35]
        };
    };
};



/*	 Core::X86::MSR::vm_cr	*/
union VM_CR_MSR
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
    int64_t	value;
};


union InterceptVector4
{ 
    struct
    {
        int32_t  intercept_vmrun : 1; // intercept VMRUN
        int32_t  intercept_vmmcall : 1;  // Intercept VMMCALL
        int32_t  pad : 30;
    };
    int32_t	value;
};
static_assert(sizeof(InterceptVector4) == 0x4, "InterceptVector4 Size Mismatch");


/*	#include <pshpack1.h> to remove struct alignment, so we wont have GDTR value issues	*/
#include <pshpack1.h>
struct DescriptorTableRegister
{
    uint16_t limit;
    uintptr_t base;
};

static_assert(sizeof(DescriptorTableRegister) == 0xA, "DESCRIPTOR_TABLE_REGISTER Size Mismatch");


struct SegmentAttribute
{
    union
    {
        uint16_t value;
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
static_assert(sizeof(uint16_t) == sizeof(SegmentAttribute), "SegmentAttribute Size Mismatch");

union ADDRESS_TRANSLATION_HELPER
{
    //
    // Indexes to locate paging-structure entries corresponds to this virtual
    // address.
    //
    struct
    {
        uint64_t Unused : 12;         //< [11:0]
        uint64_t Pt : 9;              //< [20:12]
        uint64_t Pd : 9;              //< [29:21]
        uint64_t Pdpt : 9;            //< [38:30]
        uint64_t Pml4 : 9;            //< [47:39]
    } AsIndex;

    //
    // The page offset for each type of pages. For example, for 4KB pages, bits
    // [11:0] are treated as the page offset and Mapping4Kb can be used for it.
    //
    union
    {
        uint64_t Mapping4Kb : 12;     //< [11:0]
        uint64_t Mapping2Mb : 21;     //< [20:0]
        uint64_t Mapping1Gb : 30;     //< [29:0]
    } AsPageOffset;

    uint64_t AsUInt64;
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
