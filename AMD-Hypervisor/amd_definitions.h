#pragma once
#include "includes.h"        


#define  CPUID_MAX_STANDARD_FN_NUMBER_AND_VENDOR_STRING          0x00000000
#define  CPUID_PROCESSOR_AND_PROCESSOR_FEATURE_IDENTIFIERS       0x00000001

/*	MSR stuff */
#define  MSR_APIC_BAR    0x0000001b
#define	 AMD_VM_CR	     0xC0010114
#define	 AMD_EFER	     0xC0000080 
#define  AMD_MSR_PAT     0x00000277  /*  Page Atrribute Table, see   MSR0000_0277    */
#define  VM_HSAVE_PA     0xc0010117


/*  Intercepts  */
#define INTERCEPT_VMRUN       (1UL << 0)
#define INTERCEPT_CPUID       (1UL << 18)
#define INTERCEPT_VMMCALL     (1UL << 1)
#define INTERCEPT_DB          (1UL << 1)



enum VMMCALL
{
    HYPERVISOR_SIG  = 0x1337422,
    ENABLE_HOOKS    = 0x1337423,
    END_HV          = 0x128628,
    TEST            = 0x128629,
    DISABLE_HOOKS   = 0x128630,
    DISPLAY_INFO    = 0x128631,
};


typedef struct _VMCB_CONTROL_AREA
{
    UINT16 InterceptCrRead;             // +0x000
    UINT16 InterceptCrWrite;            // +0x002
    UINT16 InterceptDrRead;             // +0x004
    UINT16 InterceptDrWrite;            // +0x006
    UINT32 InterceptException;          // +0x008
    UINT32 InterceptVec3;               // +0x00c
    UINT32 InterceptVec4;               // +0x010
    UINT8 Reserved1[0x03c - 0x014];     // +0x014 there should be intercept vector 5 here
    UINT16 PauseFilterThreshold;        // +0x03c
    UINT16 PauseFilterCount;            // +0x03e
    UINT64 IopmBasePa;                  // +0x040
    UINT64 MsrpmBasePa;                 // +0x048
    UINT64 TscOffset;                   // +0x050
    UINT32 GuestAsid;                   // +0x058
    UINT32 TlbControl;                  // +0x05c
    UINT64 VIntr;                       // +0x060
    UINT64 InterruptShadow;             // +0x068
    UINT64 ExitCode;                    // +0x070
    UINT64 ExitInfo1;                   // +0x078
    UINT64 ExitInfo2;                   // +0x080
    UINT64 ExitIntInfo;                 // +0x088
    UINT64 NpEnable;                    // +0x090
    UINT64 AvicApicBar;                 // +0x098
    UINT64 GuestPaOfGhcb;               // +0x0a0
    UINT64 EventInj;                    // +0x0a8
    UINT64 NCr3;                        // +0x0b0
    UINT64 LbrVirtualizationEnable;     // +0x0b8
    UINT64 VmcbClean;                   // +0x0c0
    UINT64 NRip;                        // +0x0c8
    UINT8 NumOfBytesFetched;            // +0x0d0
    UINT8 GuestInstructionBytes[15];    // +0x0d1
    UINT64 AvicApicBackingPagePointer;  // +0x0e0
    UINT64 Reserved2;                   // +0x0e8
    UINT64 AvicLogicalTablePointer;     // +0x0f0
    UINT64 AvicPhysicalTablePointer;    // +0x0f8
    UINT64 Reserved3;                   // +0x100
    UINT64 VmcbSaveStatePointer;        // +0x108
    UINT8 Reserved4[0x400 - 0x110];     // +0x110
} VMCB_CONTROL_AREA, * PVMCB_CONTROL_AREA;
static_assert(sizeof(VMCB_CONTROL_AREA) == 0x400,
    "VMCB_CONTROL_AREA Size Mismatch");

//
// See "VMCB Layout, State Save Area"
//
typedef struct _VMCB_STATE_SAVE_AREA
{
    UINT16 EsSelector;                  // +0x000
    UINT16 EsAttrib;                    // +0x002
    UINT32 EsLimit;                     // +0x004
    UINT64 EsBase;                      // +0x008
    UINT16 CsSelector;                  // +0x010
    UINT16 CsAttrib;                    // +0x012
    UINT32 CsLimit;                     // +0x014
    UINT64 CsBase;                      // +0x018
    UINT16 SsSelector;                  // +0x020
    UINT16 SsAttrib;                    // +0x022
    UINT32 SsLimit;                     // +0x024
    UINT64 SsBase;                      // +0x028
    UINT16 DsSelector;                  // +0x030
    UINT16 DsAttrib;                    // +0x032
    UINT32 DsLimit;                     // +0x034
    UINT64 DsBase;                      // +0x038
    UINT16 FsSelector;                  // +0x040
    UINT16 FsAttrib;                    // +0x042
    UINT32 FsLimit;                     // +0x044
    UINT64 FsBase;                      // +0x048
    UINT16 GsSelector;                  // +0x050
    UINT16 GsAttrib;                    // +0x052
    UINT32 GsLimit;                     // +0x054
    UINT64 GsBase;                      // +0x058
    UINT16 GdtrSelector;                // +0x060
    UINT16 GdtrAttrib;                  // +0x062
    UINT32 GdtrLimit;                   // +0x064
    UINT64 GdtrBase;                    // +0x068
    UINT16 LdtrSelector;                // +0x070
    UINT16 LdtrAttrib;                  // +0x072
    UINT32 LdtrLimit;                   // +0x074
    UINT64 LdtrBase;                    // +0x078
    UINT16 IdtrSelector;                // +0x080
    UINT16 IdtrAttrib;                  // +0x082
    UINT32 IdtrLimit;                   // +0x084
    UINT64 IdtrBase;                    // +0x088
    UINT16 TrSelector;                  // +0x090
    UINT16 TrAttrib;                    // +0x092
    UINT32 TrLimit;                     // +0x094
    UINT64 TrBase;                      // +0x098
    UINT8 Reserved1[0x0cb - 0x0a0];     // +0x0a0
    UINT8 Cpl;                          // +0x0cb
    UINT32 Reserved2;                   // +0x0cc
    UINT64 Efer;                        // +0x0d0
    UINT8 Reserved3[0x148 - 0x0d8];     // +0x0d8
    UINT64 Cr4;                         // +0x148
    UINT64 Cr3;                         // +0x150
    UINT64 Cr0;                         // +0x158
    UINT64 Dr7;                         // +0x160
    UINT64 Dr6;                         // +0x168
    UINT64 Rflags;                      // +0x170
    UINT64 Rip;                         // +0x178
    UINT8 Reserved4[0x1d8 - 0x180];     // +0x180
    UINT64 Rsp;                         // +0x1d8
    UINT8 Reserved5[0x1f8 - 0x1e0];     // +0x1e0
    UINT64 Rax;                         // +0x1f8
    UINT64 Star;                        // +0x200
    UINT64 LStar;                       // +0x208
    UINT64 CStar;                       // +0x210
    UINT64 SfMask;                      // +0x218
    UINT64 KernelGsBase;                // +0x220
    UINT64 SysenterCs;                  // +0x228
    UINT64 SysenterEsp;                 // +0x230
    UINT64 SysenterEip;                 // +0x238
    UINT64 Cr2;                         // +0x240
    UINT8 Reserved6[0x268 - 0x248];     // +0x248
    UINT64 GPat;                        // +0x268
    UINT64 DbgCtl;                      // +0x270
    UINT64 BrFrom;                      // +0x278
    UINT64 BrTo;                        // +0x280
    UINT64 LastExcepFrom;               // +0x288
    UINT64 LastExcepTo;                 // +0x290
} VMCB_STATE_SAVE_AREA, * PVMCB_STATE_SAVE_AREA;
static_assert(sizeof(VMCB_STATE_SAVE_AREA) == 0x298,
    "VMCB_STATE_SAVE_AREA Size Mismatch");


typedef struct _SEGMENT_DESCRIPTOR
{
    union
    {
        UINT64 AsUInt64;
        struct
        {
            UINT16 LimitLow;        // [0:15]
            UINT16 BaseLow;         // [16:31]
            UINT32 BaseMiddle : 8;  // [32:39]
            UINT32 Type : 4;        // [40:43]
            UINT32 System : 1;      // [44]
            UINT32 Dpl : 2;         // [45:46]
            UINT32 Present : 1;     // [47]
            UINT32 LimitHigh : 4;   // [48:51]
            UINT32 Avl : 1;         // [52]
            UINT32 LongMode : 1;    // [53]
            UINT32 DefaultBit : 1;  // [54]
            UINT32 Granularity : 1; // [55]
            UINT32 BaseHigh : 8;    // [56:63]
        } Fields;
    };
} SEGMENT_DESCRIPTOR, * PSEGMENT_DESCRIPTOR;
static_assert(sizeof(SEGMENT_DESCRIPTOR) == 8,
    "SEGMENT_DESCRIPTOR Size Mismatch");


/*	(Core::X86::Msr::EFER	*/
union  EFER_MSR
{
    struct {
        int		syscall : 1;
        int		reserved : 7;
        int		LongModeEnable : 1;
        int		reserved2 : 1;
        int		LongModeActive : 1;
        int		NXE : 1;
        int		SVME : 1;
        int		LMSLE : 1;
        int		FFXSE : 1;
        int		TCE : 1;
        int     reserved3 : 1;
        int     MCommit : 1;
        int     INTWB : 1;
        __int64	 reserved4 : 45;

    };
    __int64	Flags;
};

/*	 Core::X86::Msr::APIC_BAR	*/
struct APIC_BAR
{
    union
    {
        UINT64 Flags;
        struct
        {
            UINT64 Reserved1 : 8;           // [0:7]
            UINT64 BootstrapProcessor : 1;  // [8]
            UINT64 Reserved2 : 1;           // [9]
            UINT64 EnableX2ApicMode : 1;    // [10]
            UINT64 EnableXApicGlobal : 1;   // [11]
            UINT64 ApicBase : 24;           // [12:35]
        };
    };
};



/*	 Core::X86::Msr::VM_CR	*/
union 	VM_CR_MSR
{
    struct {
        int		Reserved1 : 1;
        int		InterceptInit : 1;
        int		Reserved2 : 1;
        int		SVMLock : 1;
        int		SVMEDisable : 1;
        int		Reserved3 : 27;
        int		Reserved4 : 32;
    };
    __int64	Flags;
};

/*	#include <pshpack1.h> to remove struct alignment, so we wont have GDTR value issues	*/
#pragma pack(push, 1)
typedef struct _DESCRIPTOR_TABLE_REGISTER
{
    UINT16	Limit;
    ULONG_PTR Base;
} DESCRIPTOR_TABLE_REGISTER, * PDESCRIPTOR_TABLE_REGISTER;
static_assert(sizeof(DESCRIPTOR_TABLE_REGISTER) == 0xA,
    "DESCRIPTOR_TABLE_REGISTER Size Mismatch");
#pragma pack(pop)

typedef struct _SEGMENT_ATTRIBUTE
{
    union
    {
        UINT16 AsUInt16;
        struct
        {
            UINT16 Type : 4;        // [0:3]
            UINT16 System : 1;      // [4]
            UINT16 Dpl : 2;         // [5:6]
            UINT16 Present : 1;     // [7]
            UINT16 Avl : 1;         // [8]
            UINT16 LongMode : 1;    // [9]
            UINT16 DefaultBit : 1;  // [10]
            UINT16 Granularity : 1; // [11]
            UINT16 Reserved1 : 4;   // [12:15]
        } Fields;
    };
} SEGMENT_ATTRIBUTE, * PSEGMENT_ATTRIBUTE;


/*  15.20 Event Injection   */
union EVENTINJ
{
    struct
    {
        int     Vector : 8;
        int     Type : 3;
        int     PushErrorCode : 1;
        int     Reserved : 19;
        int     Valid : 1;
        int     ErrorCode : 32;
    };
    __int64 Flags;
};


struct NPF_EXITINFO1
{
    union
    {
        UINT64 AsUInt64;
        struct
        {
            UINT64 Valid : 1;                   // [0]
            UINT64 Write : 1;                   // [1]
            UINT64 User : 1;                    // [2]
            UINT64 Reserved : 1;                // [3]
            UINT64 Execute : 1;                 // [4]
            UINT64 Reserved2 : 27;              // [5:31]
            UINT64 GuestPhysicalAddress : 1;    // [32]
            UINT64 GuestPageTables : 1;         // [33]
        } Fields;
    };
};


/*  must be 4KB aligned     */
struct VMCB
{
    VMCB_CONTROL_AREA       ControlArea;
    VMCB_STATE_SAVE_AREA    SaveStateArea;
    char pad[0x1000 - sizeof(VMCB_CONTROL_AREA) - sizeof(VMCB_STATE_SAVE_AREA)];
};

