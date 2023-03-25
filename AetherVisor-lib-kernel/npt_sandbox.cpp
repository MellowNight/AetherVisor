
#include "aethervisor_kernel.h"
#include "utils.h"

namespace Aether
{
	namespace Sandbox
	{
        extern "C"
        {
            void DenyPageAccess(void* page_addr, bool allow_reads)
            {
                svm_vmmcall(VMMCALL_ID::deny_sandbox_reads, PAGE_ALIGN(page_addr));
            }

            void DenyRegionAccess(void* base, size_t range, bool allow_reads)
            {
                for (auto offset = (uint8_t*)base; offset < (uint8_t*)base + range; offset += PAGE_SIZE)
                {
                    DenyPageAccess(base, offset);
                }
            }

            ULONG ntos_size = NULL;
            uintptr_t ntos_base = NULL;
            uintptr_t KxUnexpectedInterrupt0 = NULL;

            int SandboxPage(uintptr_t address, uintptr_t tag, bool exclude_interrupt, bool global_page)
            {
                if (global_page)
                {
                    Util::TriggerCOW((uint8_t*)address);
                }

                Util::LockPages((void*)address, IoReadAccess, KernelMode);

                if (exclude_interrupt)
                {
                    /*  KxUnexpectedInterrupt0 - 6A 00 55 E9 ?? ?? ?? ?? 6A 01 55 E9 */

                    if (ntos_base == NULL)
                    {
                        UNICODE_STRING ntos_name = RTL_CONSTANT_STRING(L"ntoskrnl.exe");

                        ntos_base = (uintptr_t)Util::GetKernelModule(&ntos_size, ntos_name);

                        KxUnexpectedInterrupt0 = Util::FindPattern(ntos_base, ntos_size, "\x6A\x00\x55\xE9\xCC\xCC\xCC\xCC\x6A\x01\x55\xE9", 12, 0xCC);
                    }
                    
                    // rcx, rdx, r8, r9

                    svm_vmmcall(VMMCALL_ID::sandbox_page, address, tag, KxUnexpectedInterrupt0);
                }
                else
                {
                    svm_vmmcall(VMMCALL_ID::sandbox_page, address, tag, NULL);
                }

                return 0;
            }


            int UnboxPage(uintptr_t address, uintptr_t tag, bool global_page = false)
            {
                if (global_page)
                {
                    Util::TriggerCOW((uint8_t*)address);
                }

                svm_vmmcall(VMMCALL_ID::unbox_page, address, tag);

                return 0;
            }

            void SandboxRegion(uintptr_t base, uintptr_t size, bool exclude_interrupt, bool COW)
            {
                for (auto offset = base; offset < base + size; offset += PAGE_SIZE)
                {
                   // LARGE_INTEGER interval;

                 //   KeDelayExecutionThread(KernelMode, FALSE, &interval);

                    SandboxPage((uintptr_t)offset, NULL, exclude_interrupt, COW);
                }
            }

            void UnboxRegion(uintptr_t base, uintptr_t size, bool global_page)
            {
                for (auto offset = base; offset < base + size; offset += PAGE_SIZE)
                {
                    UnboxPage((uintptr_t)offset, NULL, global_page);
                }
            }}
	}
}
