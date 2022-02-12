#include "Command.h"
#include "Utility.h"
#include "Logging.h"

void	HandleCommands()
{
	while (1)
	{
		switch (g_HvData->HvCommand)
		{
			for (int i = 0; i < 10; ++i)
			{
				LARGE_INTEGER	time;
				time.QuadPart = -1000000;
				KeDelayExecutionThread(KernelMode, TRUE, &time);
				logger->Log<int>("log test %i", i, TYPES::integer);
			}

			case VMMCALL::END_HV:
			{
				for (int i = 0; i < CoreCount; ++i)
				{
					KAFFINITY	Affinity = Utils::ipow(2, i);

					KeSetSystemAffinityThread(Affinity);

					svm_vmmcall(VMMCALL::END_HV, KERNEL_VMMCALL, 0, 0);
				}

				g_HvData->HvCommand = 0;

				//unmap driver

				break;
			}
			case VMMCALL::TEST:
			{
				DbgPrint("[BigBrother] vmmcall test \n");

				g_HvData->HvCommand = 0;

				break;
			}
			case VMMCALL::DISABLE_HOOKS: {
				for (int i = 0; i < CoreCount; ++i)
				{
					KAFFINITY	Affinity = Utils::ipow(2, i);

					KeSetSystemAffinityThread(Affinity);

					svm_vmmcall(VMMCALL::DISABLE_HOOKS, KERNEL_VMMCALL, 0, 0);
				}
				break;
			}
			case VMMCALL::ENABLE_HOOKS:
			{
				for (int i = 0; i < CoreCount; ++i)
				{
					KAFFINITY	Affinity = Utils::ipow(2, i);

					KeSetSystemAffinityThread(Affinity);

					svm_vmmcall(VMMCALL::ENABLE_HOOKS, KERNEL_VMMCALL, 0, 0);
				}
				break;
			}
			case VMMCALL::DISPLAY_INFO:
			{
				//Utils::DisplaySections(g_HvData->ImageStart);
				g_HvData->HvCommand = 0;
				break;
			}
			default:
			{
				break;
			}
		}
	}
}