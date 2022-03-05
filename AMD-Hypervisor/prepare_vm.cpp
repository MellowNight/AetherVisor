#include "prepare_vm.h"
#include "logging.h"

extern "C" void _sgdt(
	OUT void* Descriptor
);

extern "C" int16_t __readtr();

bool IsProcessorReadyForVmrun(Vmcb* guest_vmcb, SegmentAttribute cs_attribute)
{
	if (cs_attribute.fields.long_mode == 1)
	{
		DbgPrint("Long mode enabled\n");
	}
	else
	{
		DbgPrint("Long mode disabled\n");

	}

	MsrEfer efer_msr = { 0 };
	efer_msr.flags = __readmsr(MSR::EFER);

	if (efer_msr.svme == 1)
	{
		Logger::Log("SVME is off, invalid state! \n");
		return false;
	}

	if ((efer_msr.reserved2 != 0) || (efer_msr.reserved3 != 0) || (efer_msr.reserved4 != 0))
	{
		Logger::Log("MBZ bit of EFER is set, Invalid state! \n");
		return false;
	}

	CR0	cr0;
	cr0.Flags = __readcr0();

	if ((cr0.CacheDisable == 0) && (cr0.NotWriteThrough == 1))
	{
		Logger::Log("CR0.CD is zero and CR0.NW is set. \n");
		return false;
	}

	if (cr0.Reserved4 != 0)
	{
		Logger::Log("CR0[63:32] are not zero. \n");
		return false;
	}
	
	RFLAGS rflags;
	rflags.Flags = __readeflags();

	CR3	cr3;
	CR4	cr4;

	cr3.Flags = __readcr3();
	cr4.Flags = __readcr4();

	if (rflags.Virtual8086ModeFlag == 1 && (cr4.Flags << 23 & 1))
	{
		Logger::Log("CR4.CET=1 and U_CET.SS=1 when EFLAGS.VM=1 \n");
	}

	if ((cr3.Reserved1 != 0) || (cr3.Reserved2 != 0) || (cr4.Reserved1 != 0)
		|| (cr4.Reserved2 != 0) || (cr4.Reserved3 != 0) || (cr4.Reserved4 != 0))
	{
		Logger::Log("cr3 or cr4 MBZ bits are zero. Invalid state rn \n");
		return false;
	}

	DR6	dr6;
	DR7 dr7;

	dr6.Flags = __readdr(6);
	dr7.Flags = __readdr(7);

	if ((dr6.Flags & (0xFFFFFFFF00000000)) || (dr7.Reserved4 != 0))
	{
		Logger::Log("DR6[63:32] are not zero, or DR7[63:32] are not zero.Invalid State!\n");
		return false;
	}

	if (cr0.PagingEnable == 0)
	{
		Logger::Log("Paging disabled, Invalid state! \n");
		return false;
	}

	if (efer_msr.long_mode_enable == 1 && cr0.PagingEnable == 1)
	{
		if (cr4.PhysicalAddressExtension == 0)
		{
			Logger::Log("EFER.LME and CR0.PG are both set and CR4.PAE is zero, Invalid state! \n");
			return false;
		}

		if (cr0.ProtectionEnable == 0)
		{
			Logger::Log("EFER.LME and CR0.PG are both non-zero and CR0.PE is zero, Invalid state! \n");
			return false;
		}

		if (cs_attribute.fields.long_mode != 0 && cs_attribute.fields.long_mode != 0)
		{
			Logger::Log("EFER.LME, CR0.PG, CR4.PAE, CS.L, and CS.D are all non-zero. \n");
			return false;
		}
	}

	if (guest_vmcb->control_area.GuestAsid == 0)
	{
		Logger::Log("ASID is equal to zero. Invalid guest state \n");
		return false;
	}

	if (!(guest_vmcb->control_area.InterceptVec4 & 1))
	{
		Logger::Log("The VMRUN intercept bit is clear. Invalid state! \n");
		return false;
	}

	Logger::Log("consistency checks passed \n");
	return true;

	/*	to do: msr and IOIO map address checks, and some more. */
}

/*	Copy bits bits 55:52 and 47:40 from segment descriptor	*/
SegmentAttribute GetSegmentAttributes(uint16_t segment_selector, uintptr_t gdt_base)
{
	SEGMENT_SELECTOR selector;
	selector.Flags = segment_selector;

	SegmentDescriptor seg_descriptor = ((SegmentDescriptor*)gdt_base)[selector.Index];

	SegmentAttribute attribute;

	attribute.fields.type = seg_descriptor.Type;
	attribute.fields.system = seg_descriptor.System;
	attribute.fields.dpl = seg_descriptor.Dpl;
	attribute.fields.present = seg_descriptor.Present;
	attribute.fields.avl = seg_descriptor.Avl;
	attribute.fields.long_mode = seg_descriptor.LongMode;
	attribute.fields.default_bit = seg_descriptor.DefaultBit;
	attribute.fields.granularity = seg_descriptor.Granularity;
	attribute.fields.reserved1 = 0;

	return attribute;
}

typedef struct
{
	/**
	 * @brief Segment limit field (15:00)
	 *
	 * Specifies the size of the segment. The processor puts together the two segment limit fields to form a 20-bit value. The
	 * processor interprets the segment limit in one of two ways, depending on the setting of the G (granularity) flag:
	 * - If the granularity flag is clear, the segment size can range from 1 byte to 1 MByte, in byte increments.
	 * - If the granularity flag is set, the segment size can range from 4 KBytes to 4 GBytes, in 4-KByte increments.
	 * The processor uses the segment limit in two different ways, depending on whether the segment is an expand-up or an
	 * expand-down segment. For expand-up segments, the offset in a logical address can range from 0 to the segment limit.
	 * Offsets greater than the segment limit generate general-protection exceptions (\#GP, for all segments other than SS) or
	 * stack-fault exceptions (\#SS for the SS segment). For expand-down segments, the segment limit has the reverse function;
	 * the offset can range from the segment limit plus 1 to FFFFFFFFH or FFFFH, depending on the setting of the B flag.
	 * Offsets less than or equal to the segment limit generate general-protection exceptions or stack-fault exceptions.
	 * Decreasing the value in the segment limit field for an expanddown segment allocates new memory at the bottom of the
	 * segment's address space, rather than at the top. IA-32 architecture stacks always grow downwards, making this mechanism
	 * convenient for expandable stacks.
	 *
	 * @see Vol3A[3.4.5.1(Code- and Data-Segment Descriptor Types)]
	 */
	uint16_t segment_limit_low;

	/**
	 * @brief Base address field (15:00)
	 *
	 * Defines the location of byte 0 of the segment within the 4-GByte linear address space. The processor puts together the
	 * three base address fields to form a single 32-bit value. Segment base addresses should be aligned to 16-byte boundaries.
	 * Although 16-byte alignment is not required, this alignment allows programs to maximize performance by aligning code and
	 * data on 16-byte boundaries.
	 */
	uint16_t base_address_low;
	/**
	 * @brief Segment descriptor fields
	 */
	union
	{
		struct
		{
			/**
			 * [Bits 7:0] Base address field (23:16); see description of $BASE_LOW for more details.
			 */
			uint32_t base_address_middle : 8;
#define SEGMENT__BASE_ADDRESS_MIDDLE_BIT                             16
#define SEGMENT__BASE_ADDRESS_MIDDLE_FLAG                            0xFF
#define SEGMENT__BASE_ADDRESS_MIDDLE_MASK                            0xFF
#define SEGMENT__BASE_ADDRESS_MIDDLE(_)                              (((_) >> 0) & 0xFF)

			/**
			 * @brief Type field
			 *
			 * [Bits 11:8] Indicates the segment or gate type and specifies the kinds of access that can be made to the segment and the
			 * direction of growth. The interpretation of this field depends on whether the descriptor type flag specifies an
			 * application (code or data) descriptor or a system descriptor. The encoding of the type field is different for code,
			 * data, and system descriptors.
			 *
			 * @see Vol3A[3.4.5.1(Code- and Data-Segment Descriptor Types)]
			 */
			uint32_t type : 4;
#define SEGMENT__TYPE_BIT                                            8
#define SEGMENT__TYPE_FLAG                                           0xF00
#define SEGMENT__TYPE_MASK                                           0x0F
#define SEGMENT__TYPE(_)                                             (((_) >> 8) & 0x0F)

			/**
			 * @brief S (descriptor type) flag
			 *
			 * [Bit 12] Specifies whether the segment descriptor is for a system segment (S flag is clear) or a code or data segment (S
			 * flag is set).
			 */
			uint32_t descriptor_type : 1;
#define SEGMENT__DESCRIPTOR_TYPE_BIT                                 12
#define SEGMENT__DESCRIPTOR_TYPE_FLAG                                0x1000
#define SEGMENT__DESCRIPTOR_TYPE_MASK                                0x01
#define SEGMENT__DESCRIPTOR_TYPE(_)                                  (((_) >> 12) & 0x01)

			/**
			 * @brief DPL (descriptor privilege level) field
			 *
			 * [Bits 14:13] Specifies the privilege level of the segment. The privilege level can range from 0 to 3, with 0 being the
			 * most privileged level. The DPL is used to control access to the segment. See Section 5.5, "Privilege Levels", for a
			 * description of the relationship of the DPL to the CPL of the executing code segment and the RPL of a segment selector.
			 */
			uint32_t descriptor_privilege_level : 2;
#define SEGMENT__DESCRIPTOR_PRIVILEGE_LEVEL_BIT                      13
#define SEGMENT__DESCRIPTOR_PRIVILEGE_LEVEL_FLAG                     0x6000
#define SEGMENT__DESCRIPTOR_PRIVILEGE_LEVEL_MASK                     0x03
#define SEGMENT__DESCRIPTOR_PRIVILEGE_LEVEL(_)                       (((_) >> 13) & 0x03)

			/**
			 * @brief P (segment-present) flag
			 *
			 * [Bit 15] Indicates whether the segment is present in memory (set) or not present (clear). If this flag is clear, the
			 * processor generates a segment-not-present exception (\#NP) when a segment selector that points to the segment descriptor
			 * is loaded into a segment register. Memory management software can use this flag to control which segments are actually
			 * loaded into physical memory at a given time. It offers a control in addition to paging for managing virtual memory.
			 */
			uint32_t present : 1;
#define SEGMENT__PRESENT_BIT                                         15
#define SEGMENT__PRESENT_FLAG                                        0x8000
#define SEGMENT__PRESENT_MASK                                        0x01
#define SEGMENT__PRESENT(_)                                          (((_) >> 15) & 0x01)

			/**
			 * [Bits 19:16] Segment limit field (19:16); see description of $LIMIT_LOW for more details.
			 */
			uint32_t segment_limit_high : 4;
#define SEGMENT__SEGMENT_LIMIT_HIGH_BIT                              16
#define SEGMENT__SEGMENT_LIMIT_HIGH_FLAG                             0xF0000
#define SEGMENT__SEGMENT_LIMIT_HIGH_MASK                             0x0F
#define SEGMENT__SEGMENT_LIMIT_HIGH(_)                               (((_) >> 16) & 0x0F)

			/**
			 * @brief Available bit
			 *
			 * [Bit 20] Bit 20 of the second doubleword of the segment descriptor is available for use by system software.
			 */
			uint32_t system : 1;
#define SEGMENT__SYSTEM_BIT                                          20
#define SEGMENT__SYSTEM_FLAG                                         0x100000
#define SEGMENT__SYSTEM_MASK                                         0x01
#define SEGMENT__SYSTEM(_)                                           (((_) >> 20) & 0x01)

			/**
			 * @brief L (64-bit code segment) flag
			 *
			 * [Bit 21] In IA-32e mode, bit 21 of the second doubleword of the segment descriptor indicates whether a code segment
			 * contains native 64-bit code. A value of 1 indicates instructions in this code segment are executed in 64-bit mode. A
			 * value of 0 indicates the instructions in this code segment are executed in compatibility mode. If L-bit is set, then
			 * D-bit must be cleared. When not in IA-32e mode or for non-code segments, bit 21 is reserved and should always be set to
			 * 0.
			 */
			uint32_t long_mode : 1;
#define SEGMENT__LONG_MODE_BIT                                       21
#define SEGMENT__LONG_MODE_FLAG                                      0x200000
#define SEGMENT__LONG_MODE_MASK                                      0x01
#define SEGMENT__LONG_MODE(_)                                        (((_) >> 21) & 0x01)

			/**
			 * @brief D/B (default operation size/default stack pointer size and/or upper bound) flag
			 *
			 * [Bit 22] Performs different functions depending on whether the segment descriptor is an executable code segment, an
			 * expand-down data segment, or a stack segment. (This flag should always be set to 1 for 32-bit code and data segments and
			 * to 0 for 16-bit code and data segments.)
			 * - Executable code segment. The flag is called the D flag and it indicates the default length for effective addresses and
			 * operands referenced by instructions in the segment. If the flag is set, 32-bit addresses and 32-bit or 8-bit operands
			 * are assumed; if it is clear, 16-bit addresses and 16-bit or 8-bit operands are assumed. The instruction prefix 66H can
			 * be used to select an operand size other than the default, and the prefix 67H can be used select an address size other
			 * than the default.
			 * - Stack segment (data segment pointed to by the SS register). The flag is called the B (big) flag and it specifies the
			 * size of the stack pointer used for implicit stack operations (such as pushes, pops, and calls). If the flag is set, a
			 * 32-bit stack pointer is used, which is stored in the 32-bit ESP register; if the flag is clear, a 16-bit stack pointer
			 * is used, which is stored in the 16- bit SP register. If the stack segment is set up to be an expand-down data segment
			 * (described in the next paragraph), the B flag also specifies the upper bound of the stack segment.
			 * - Expand-down data segment. The flag is called the B flag and it specifies the upper bound of the segment. If the flag
			 * is set, the upper bound is FFFFFFFFH (4 GBytes); if the flag is clear, the upper bound is FFFFH (64 KBytes).
			 */
			uint32_t default_big : 1;
#define SEGMENT__DEFAULT_BIG_BIT                                     22
#define SEGMENT__DEFAULT_BIG_FLAG                                    0x400000
#define SEGMENT__DEFAULT_BIG_MASK                                    0x01
#define SEGMENT__DEFAULT_BIG(_)                                      (((_) >> 22) & 0x01)

			/**
			 * @brief G (granularity) flag
			 *
			 * [Bit 23] Determines the scaling of the segment limit field. When the granularity flag is clear, the segment limit is
			 * interpreted in byte units; when flag is set, the segment limit is interpreted in 4-KByte units. (This flag does not
			 * affect the granularity of the base address; it is always byte granular.) When the granularity flag is set, the twelve
			 * least significant bits of an offset are not tested when checking the offset against the segment limit. For example, when
			 * the granularity flag is set, a limit of 0 results in valid offsets from 0 to 4095.
			 */
			uint32_t granularity : 1;
#define SEGMENT__GRANULARITY_BIT                                     23
#define SEGMENT__GRANULARITY_FLAG                                    0x800000
#define SEGMENT__GRANULARITY_MASK                                    0x01
#define SEGMENT__GRANULARITY(_)                                      (((_) >> 23) & 0x01)

			/**
			 * [Bits 31:24] Base address field (31:24); see description of $BASE_LOW for more details.
			 */
			uint32_t base_address_high : 8;
#define SEGMENT__BASE_ADDRESS_HIGH_BIT                               24
#define SEGMENT__BASE_ADDRESS_HIGH_FLAG                              0xFF000000
#define SEGMENT__BASE_ADDRESS_HIGH_MASK                              0xFF
#define SEGMENT__BASE_ADDRESS_HIGH(_)                                (((_) >> 24) & 0xFF)
		};

		uint32_t flags;
	};


	/**
	 * Base address field (32:63); see description of $BASE_LOW for more details.
	 */
	uint32_t base_address_upper;
#define SEGMENT__BASE_ADDRESS_SHIFT 32

	/**
	 * Base address field (32:63); see description of $BASE_LOW for more details.
	 */
	uint32_t must_be_zero;
} segment_descriptor_64;

void SetupTssIst()
{	
	DescriptorTableRegister	gdtr;
	_sgdt(&gdtr);

	SEGMENT_SELECTOR selector;
	selector.Flags = __readtr();

	auto seg_descriptor = *reinterpret_cast<segment_descriptor_64*>(
		gdtr.base + (selector.Index * 8));

	auto base = (uintptr_t)(
		seg_descriptor.base_address_low +
		(seg_descriptor.base_address_middle << SEGMENT__BASE_ADDRESS_MIDDLE_BIT) +
		(seg_descriptor.base_address_high << SEGMENT__BASE_ADDRESS_HIGH_BIT)
	);

	if (!seg_descriptor.descriptor_type)
	{
		base += ((uint64_t)seg_descriptor.base_address_upper << 32);
	}

	auto tss_base = (TaskStateSegment*)base;

	DbgPrint("gdtr.base %p \n", gdtr.base);
	DbgPrint("selector.Index %p \n", selector.Index);
	DbgPrint("seg_descriptor.baselow %p \n", seg_descriptor.base_address_low);
	DbgPrint("seg_descriptor.BaseMiddle %p \n", seg_descriptor.base_address_middle);
	DbgPrint("seg_descriptor.BaseHigh %p \n", seg_descriptor.base_address_high);

	/*	Find free IST entry for page fault	*/

	int idx = 0;
	for (idx = 0; idx < 7; ++idx)
	{
		DbgPrint("idx %d \n", idx);

		if (!tss_base->ist[idx])
		{
			break;
		}
	}

	auto pf_stack = ExAllocatePoolZero(NonPagedPool, PAGE_SIZE * 16, 'abcd');

	tss_base->ist[idx] = (uint8_t*)pf_stack + PAGE_SIZE * 16;

	DescriptorTableRegister	idtr;
	__sidt(&idtr);

	DbgPrint("IDT base %p \n", idtr.base);
	DbgPrint("page fault stack %p \n", pf_stack);
	DbgPrint("&((InterruptDescriptor64*)idtr.base)[0xE] %p \n", &((InterruptDescriptor64*)idtr.base)[0xE]);


	auto irql = Utils::DisableWP();

	((InterruptDescriptor64*)idtr.base)[0xE].ist = idx; 

	Utils::EnableWP(irql);
	__debugbreak();
}

void SetupMSRPM(CoreVmcbData* core_data)
{
	size_t bits_per_msr = 16000 / 8000;
	size_t bits_per_byte = sizeof(uint8_t) / 8;
	size_t msrpm_size = PAGE_SIZE * 2;

	auto msrpm = ExAllocatePoolZero(NonPagedPool, msrpm_size, 'msr0');

	core_data->guest_vmcb.control_area.MsrpmBasePa = MmGetPhysicalAddress(msrpm).QuadPart;

	RTL_BITMAP bitmap;

    RtlInitializeBitMap(&bitmap, (PULONG)msrpm, msrpm_size * 8);
    RtlClearAllBits(&bitmap);

	auto section2_offset = (0x800 * bits_per_byte);
	auto efer_offset = section2_offset + (bits_per_msr * (MSR::EFER - 0xC0000000));

	/*	intercept EFER read and write	*/
    RtlSetBits(&bitmap, efer_offset, 2);
}

void ConfigureProcessor(CoreVmcbData* core_data, CONTEXT* context_record)
{
	core_data->guest_vmcb_physicaladdr = MmGetPhysicalAddress(&core_data->guest_vmcb).QuadPart;
	core_data->host_vmcb_physicaladdr = MmGetPhysicalAddress(&core_data->host_vmcb).QuadPart;
	core_data->self = core_data;

	core_data->guest_vmcb.control_area.NCr3 = hypervisor->normal_ncr3;
	core_data->guest_vmcb.control_area.NpEnable = (1UL << 0);

	DescriptorTableRegister	gdtr, idtr;

	_sgdt(&gdtr);
	__sidt(&idtr);

	InterceptVector4 intercept_vector4;

	intercept_vector4.intercept_vmmcall = 1;
	intercept_vector4.intercept_vmrun = 1;

	core_data->guest_vmcb.control_area.InterceptVec4 = intercept_vector4.as_int32;

	InterceptVector2 intercept_vector2;

	intercept_vector2.intercept_pf = 1;
	intercept_vector2.intercept_bp = 1;

	core_data->guest_vmcb.control_area.InterceptException = intercept_vector2.as_int32;

	core_data->guest_vmcb.control_area.GuestAsid = 1;
	core_data->guest_vmcb.save_state_area.Cr0 = __readcr0();
	core_data->guest_vmcb.save_state_area.Cr2 = __readcr2();
	core_data->guest_vmcb.save_state_area.Cr3 = __readcr3();
	core_data->guest_vmcb.save_state_area.Cr4 = __readcr4();


	core_data->guest_vmcb.save_state_area.Rip = context_record->Rip;
	core_data->guest_vmcb.save_state_area.Rax = context_record->Rax;
	core_data->guest_vmcb.save_state_area.Rsp = context_record->Rsp;
	core_data->guest_vmcb.save_state_area.Rflags = __readeflags();
	core_data->guest_vmcb.save_state_area.Efer = __readmsr(MSR::EFER);
	core_data->guest_vmcb.save_state_area.GPat = __readmsr(MSR::PAT);

	core_data->guest_vmcb.save_state_area.GdtrLimit = gdtr.limit;
	core_data->guest_vmcb.save_state_area.GdtrBase = gdtr.base;
	core_data->guest_vmcb.save_state_area.IdtrLimit = idtr.limit;
	core_data->guest_vmcb.save_state_area.IdtrBase = idtr.base;

	core_data->guest_vmcb.save_state_area.CsLimit = GetSegmentLimit(context_record->SegCs);
	core_data->guest_vmcb.save_state_area.DsLimit = GetSegmentLimit(context_record->SegDs);
	core_data->guest_vmcb.save_state_area.EsLimit = GetSegmentLimit(context_record->SegEs);
	core_data->guest_vmcb.save_state_area.SsLimit = GetSegmentLimit(context_record->SegSs);

	core_data->guest_vmcb.save_state_area.CsSelector = context_record->SegCs;
	core_data->guest_vmcb.save_state_area.DsSelector = context_record->SegDs;
	core_data->guest_vmcb.save_state_area.EsSelector = context_record->SegEs;
	core_data->guest_vmcb.save_state_area.SsSelector = context_record->SegSs;

	core_data->guest_vmcb.save_state_area.CsAttrib = GetSegmentAttributes(context_record->SegCs, gdtr.base).as_uint16;
	core_data->guest_vmcb.save_state_area.DsAttrib = GetSegmentAttributes(context_record->SegDs, gdtr.base).as_uint16;
	core_data->guest_vmcb.save_state_area.EsAttrib = GetSegmentAttributes(context_record->SegEs, gdtr.base).as_uint16;
	core_data->guest_vmcb.save_state_area.SsAttrib = GetSegmentAttributes(context_record->SegSs, gdtr.base).as_uint16;

	// SetupTssIst();
	
	Logger::Log("core_data: %p\n", core_data);

	__svm_vmsave(core_data->guest_vmcb_physicaladdr);

	__writemsr(VM_HSAVE_PA, MmGetPhysicalAddress(&core_data->host_save_area).QuadPart);

	__svm_vmsave(core_data->host_vmcb_physicaladdr);
}

bool IsSvmSupported()
{
	int32_t	cpu_info[4] = { 0 };

	__cpuid(cpu_info, CPUID::PROCESSOR_AND_FEATURE_IDENTIFIERS);

	if ((cpu_info[2] & (1 << 1)) == 0)
	{
		return false;
	}

	int32_t vendor_name_result[4];

	char vendor_name[13];

	__cpuid(vendor_name_result, CPUID::MAX_STANDARD_FN_NUMBER_AND_VENDOR_STRING);
	memcpy(vendor_name, &vendor_name_result[1], sizeof(int));
	memcpy(vendor_name + 4, &vendor_name_result[3], sizeof(int));
	memcpy(vendor_name + 8, &vendor_name_result[2], sizeof(int));

	vendor_name[12] = '\0';

	Logger::Log("[SETUP] Vendor Name %s \n", vendor_name);

	if (strcmp(vendor_name, "AuthenticAMD") && strcmp(vendor_name, "VmwareVmware"))
	{
		return false;
	}

	return true;
}

bool IsSvmUnlocked()
{
	MsrVmcr	msr;
	msr.flags = __readmsr(MSR::VM_CR);


	if (msr.svm_lock == 0)
	{
		msr.svme_disable = 0;
		msr.svm_lock = 1;
		__writemsr(MSR::VM_CR, msr.flags);
	}
	else if (msr.svme_disable == 1)
	{
		return false;
	}

	return true;
}

void EnableSvme()
{
	MsrEfer	msr;
	msr.flags = __readmsr(MSR::EFER);
	msr.svme = 1;
	__writemsr(MSR::EFER, msr.flags);
}