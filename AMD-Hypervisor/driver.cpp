#include "virtualization.h"

int	core_count = 0;
CoreVmcbData* g_VpData[32] = { 0 };


extern "C" void NTAPI LaunchVm(void* VmLaunchParams);



bool IsHypervisorPresent(int CoreNumber)
{
	/*	shitty check, switched from vmmcall to pointer check to avoid #UD	*/

	if (g_VpData[CoreNumber] != NULL)
	{
		return true;
	}
	else
	{
		return false;
	}
}

bool IsProcessorReadyForVmrun(VMCB* GuestVmcb, SegmentAttribute CsAttr)
{
	MsrEfer efer_msr = { 0 };
	efer_msr.Flags = __readmsr(AMD_EFER);

	if (efer_msr.SVME == 1)
	{
		DbgPrint("SVME is off, invalid state! \n");
		return false;
	}

	if ((efer_msr.reserved2 != 0) || (efer_msr.reserved3 != 0) || (efer_msr.reserved4 != 0))
	{
		DbgPrint("MBZ bit of EFER is set, Invalid state! \n");
		return false;
	}

	CR0	cr0;
	cr0.Flags = __readcr0();

	if ((cr0.CacheDisable == 0) && (cr0.NotWriteThrough == 1))
	{
		DbgPrint("CR0.CD is zero and CR0.NW is set. \n");
		return false;
	}

	if (cr0.Reserved4 != 0)
	{
		DbgPrint("CR0[63:32] are not zero. \n");
		return false;
	}

	RFLAGS	rflags;
	rflags.Flags = __readeflags();

	CR3	cr3;
	CR4	cr4;

	cr3.Flags = __readcr3();
	cr4.Flags = __readcr4();

	if (rflags.Virtual8086ModeFlag == 1 && (cr4.Flags << 23 & 1))
	{
		DbgPrint("CR4.CET=1 and U_CET.SS=1 when EFLAGS.VM=1 \n");
	}

	if ((cr3.Reserved1 != 0) || (cr3.Reserved2 != 0) || (cr4.Reserved1 != 0)
		|| (cr4.Reserved2 != 0) || (cr4.Reserved3 != 0) || (cr4.Reserved4 != 0))
	{
		DbgPrint("cr3 or cr4 MBZ bits are zero. Invalid state rn \n");
		return false;
	}

	DR6	dr6;
	DR7 dr7;

	dr6.Flags = __readdr(6);
	dr7.Flags = __readdr(7);

	if ((dr6.Flags & (0xFFFFFFFF00000000)) || (dr7.Reserved4 != 0))
	{
		DbgPrint("DR6[63:32] are not zero, or DR7[63:32] are not zero. Invalid State! \n");
		return false;
	}

	if (cr0.PagingEnable == 0)
	{
		DbgPrint("Paging disabled, Invalid state! \n");
		return false;
	}

	if (efer_msr.long_mode_enable == 1 && cr0.PagingEnable == 1)
	{
		if (cr4.PhysicalAddressExtension == 0)
		{
			DbgPrint("EFER.LME and CR0.PG are both set and CR4.PAE is zero, Invalid state! \n");
			return false;
		}

		if (cr0.ProtectionEnable == 0)
		{
			DbgPrint("EFER.LME and CR0.PG are both non-zero and CR0.PE is zero, Invalid state! \n");
			return false;
		}

		if (CsAttr.Fields.LongMode != 0 && CsAttr.Fields.LongMode != 0)
		{
			DbgPrint("EFER.LME, CR0.PG, CR4.PAE, CS.L, and CS.D are all non-zero. \n");
			return false;
		}
	}

	if (GuestVmcb->ControlArea.GuestAsid == 0)
	{
		DbgPrint("ASID is equal to zero. Invalid guest state \n");
		return false;
	}

	if (!(GuestVmcb->ControlArea.InterceptVec4 & 1))
	{
		DbgPrint("The VMRUN intercept bit is clear. Invalid state! \n");
		return false;
	}

	DbgPrint("consistency checks passed \n");
	return true;

	/*	to do: msr and IOIO map address checks, and some more. */
}

HYPERVISOR_DATA* g_HvData = 0;

BOOL	VirtualizeAllProcessors()
{
	if (!IsSvmSupported())
	{
		KeBugCheck(MANUALLY_INITIATED_CRASH);
		DbgPrint("[SETUP] SVM isn't supported on this processor! \n");
		return FALSE;
	}

	if (!IsSvmUnlocked())
	{
		KeBugCheck(MANUALLY_INITIATED_CRASH);
		DbgPrint("[SETUP] SVM operation is locked off in BIOS! \n");
		return FALSE;
	}

	g_HvData = (HYPERVISOR_DATA*)ExAllocatePoolZero(NonPagedPool, sizeof(HYPERVISOR_DATA), 'HvDa');

	BuildNestedPagingTables(&g_HvData->PrimaryNCr3, true);
	BuildNestedPagingTables(&g_HvData->SecondaryNCr3, false);
	BuildNestedPagingTables(&g_HvData->TertiaryCr3, true);

	DbgPrint("imagestart %p \n", g_HvData->ImageStart);
	DbgPrint("ImageSize %d \n", g_HvData->ImageSize);


	CoreCount = KeQueryActiveProcessorCount(0);

	for (int i = 0; i < CoreCount; ++i)
	{
		KAFFINITY	Affinity = Utils::ipow(2, i);

		KeSetSystemAffinityThread(Affinity);

		DbgPrint("============================================================================= \n");
		DbgPrint("[SETUP] amount of active processors %i \n", CoreCount);
		DbgPrint("[SETUP] Currently running on core %i \n", i);


		CONTEXT* pContext = (CONTEXT*)ExAllocatePoolZero(NonPagedPool, sizeof(CONTEXT), 'Cotx');

		RtlCaptureContext(pContext);

		if (IsHypervisorPresent(i) == false)
		{
			Enable_Svme();

			g_VpData[i] = (CoreVmcbData*)ExAllocatePoolZero(NonPagedPool, sizeof(CoreVmcbData), 'Vmcb');

			ConfigureProcessor(g_VpData[i], pContext, g_HvData);

			SegmentAttribute	CsAttrib;

			CsAttrib.AsUInt16 = g_VpData[i]->GuestVmcb.SaveStateArea.CsAttrib;

			if (IsProcessorReadyForVmrun(&g_VpData[i]->GuestVmcb, CsAttrib))
			{
				LaunchVm(&g_VpData[i]->guest_vmcb_physicaladdr);
			}
			else
			{
				DbgPrint("[SETUP] A problem occured!! invalid guest state \n");
				__debugbreak();
			}
		}
		else
		{
			DbgPrint("===================== Hypervisor Successfully Launched rn !! =============================\n \n");
		}
	}

	logger = logger->Initialize();

	return TRUE;
}

BOOL LaunchStatus = TRUE;
void	HypervisorEntry()
{
	LaunchStatus = VirtualizeAllProcessors();

	PsTerminateSystemThread(STATUS_SUCCESS);
}


void LogInfo()
{
	//while (g_HvData->ImageStart == FALSE)
	//{
	//	g_HvData->ImageStart = Utils::GetDriverBaseAddress(&g_HvData->ImageSize, HV_ANALYZED_IMAGE);
	//}

	while (g_HvData->TargetPID == 0)
	{
		LARGE_INTEGER	time;
		time.QuadPart = -10000000;
		KeDelayExecutionThread(KernelMode, TRUE, &time);
		g_HvData->TargetPID = Utils::GetProcessID(HvAnalyzedImage2);
	}

	DbgPrint("[BigBrother] Found target process! PID %i \n", g_HvData->TargetPID);


	LARGE_INTEGER	time;
	time.QuadPart = -1000000; // 100 milliseconds
	KeDelayExecutionThread(KernelMode, TRUE, &time);

	// FindSignatures();
	svm_vmmcall(VMMCALL::ENABLE_HOOKS, KERNEL_VMMCALL, 0, 0);


	DbgPrint("[BigBrother] Our target HV_ANALYZED_IMAGE Found!!  g_HvData->ImageStart  %p ImageSize %p \n",
		g_HvData->ImageStart, g_HvData->ImageSize);

	//LaunchExperiments();

	while (1)
	{
		//logger->Flush();
	}
}

NTSTATUS DriverUnload(PDRIVER_OBJECT DriverObject)
{
	DbgPrint("[Big Brother] - Devirtualizing system, Driver unloading!\n");

	return STATUS_SUCCESS;
}

NTSTATUS MapperEntry(PDRIVER_OBJECT DriverObject, PUNICODE_STRING RegistryPath)
{
	DbgPrint("MapperEntry \n");

	HANDLE	hThread;
	PsCreateSystemThread(&hThread, GENERIC_ALL, NULL, NULL, NULL, 
		(PKSTART_ROUTINE)HypervisorEntry, NULL);
	
	LARGE_INTEGER	time;
	time.QuadPart = -10000000;
	KeDelayExecutionThread(KernelMode, TRUE, &time);
	
	HANDLE	LogThread;
	PsCreateSystemThread(&LogThread, GENERIC_ALL, NULL, NULL, NULL,
		(PKSTART_ROUTINE)LogInfo, NULL);


	HANDLE	CommandThread;

	PsCreateSystemThread(&CommandThread, GENERIC_ALL, NULL, NULL, NULL,
		(PKSTART_ROUTINE)HandleCommands, NULL);

	// never do anything directly in mapper entry. it always causes extremely weird problems. Do everything in newly created system threads.

	return	STATUS_SUCCESS;
}