#include "vmexit.h"
#include "npt_sandbox.h"

/*  HandleVmmcall only handles the vmmcall for 1 core.
    It is the guest's responsibility to set thread affinity.
*/
void HandleVmmcall(VcpuData* vmcb_data, GeneralRegisters* GuestRegisters, bool* EndVM)
{
    auto id = GuestRegisters->rcx;

    switch (id)
    {
    case VMMCALL_ID::register_sandbox:
    {
        auto handler_id = GuestRegisters->rdx;

        if (handler_id == Sandbox::readwrite_handler || handler_id == Sandbox::execute_handler)
        {
            Sandbox::sandbox_hooks[handler_id] = (void*)GuestRegisters->r8;
        }
        else
        {
            /*  invalid input   */
            __debugbreak();
        }

        break;
    }
    case VMMCALL_ID::sandbox_page:
    {
        Sandbox::AddPageToSandbox(
            vmcb_data,
            (void*)GuestRegisters->rdx,
            GuestRegisters->r8
        );

        break;
    }
    case VMMCALL_ID::remap_page_ncr3_specific:
    {
        auto address = (void*)GuestRegisters->rdx;
        auto copy_page = (uint8_t*)GuestRegisters->r8;
        int core_id = GuestRegisters->r9;

        auto vmroot_cr3 = __readcr3();

        __writecr3(vmcb_data->guest_vmcb.save_state_area.Cr3);

        auto hook_entry = &NPTHooks::npt_hook_array[NPTHooks::hook_count];

        if ((uintptr_t)address < 0x7FFFFFFFFFF)
        {
            hook_entry->mdl = PageUtils::LockPages(address, IoReadAccess, UserMode);
        }
        else
        {
            hook_entry->mdl = PageUtils::LockPages(address, IoReadAccess, KernelMode);
        }

        auto physical_page = PAGE_ALIGN(MmGetPhysicalAddress(address).QuadPart);

        NPTHooks::hook_count += 1;

        hook_entry->active = true;
        hook_entry->process_cr3 = vmcb_data->guest_vmcb.save_state_area.Cr3;

        /*	get the guest pte and physical address of the hooked page	*/

        hook_entry->guest_physical_page = (uint8_t*)physical_page;

        hook_entry->guest_pte = PageUtils::GetPte((void*)address, hook_entry->process_cr3);

        hook_entry->original_nx = hook_entry->guest_pte->ExecuteDisable;

        hook_entry->guest_pte->ExecuteDisable = 0;
        hook_entry->guest_pte->Write = 1;


        /*	get the nested pte of the guest physical address, and set its PFN to the copy page in kernel	*/

        hook_entry->hookless_npte = PageUtils::GetPte((void*)physical_page, Hypervisor::Get()->ncr3_dirs[core_id]);

        hook_entry->hookless_npte->PageFrameNumber = hook_entry->hooked_pte->PageFrameNumber;

        auto hooked_copy = PageUtils::VirtualAddrFromPfn(hook_entry->hooked_pte->PageFrameNumber);


        /*	place the new memory in our copy page here	*/

        memcpy((uint8_t*)hooked_copy, copy_page, PAGE_SIZE);

        /*	SetNptHook epilogue	*/

        vmcb_data->guest_vmcb.control_area.TlbControl = 3;

        __writecr3(vmroot_cr3);

        break;
    }
    case VMMCALL_ID::remove_npt_hook:
    {
        auto vmroot_cr3 = __readcr3();

        __writecr3(vmcb_data->guest_vmcb.save_state_area.Cr3);

        NPTHooks::ForEachHook(
            [](NPTHooks::NptHook* hook_entry, void* data)-> bool {

                if (hook_entry->tag == (uintptr_t)data)
                {
                    NPTHooks::UnsetHook(hook_entry);
                }
                return true;
            },
            (void*)GuestRegisters->rdx
        );

        __writecr3(vmroot_cr3);

        break;
    }
    case VMMCALL_ID::set_npt_hook:
    {			
        DbgPrint("noexecute_cr3 id = %i \n \n", GuestRegisters->r12);

        NPTHooks::SetNptHook(
            vmcb_data,
            (void*)GuestRegisters->rdx,
            (uint8_t*)GuestRegisters->r8,
            GuestRegisters->r9,
            GuestRegisters->r12,
            GuestRegisters->r11
        );

        break;
    }
    case VMMCALL_ID::disable_hv:
    {
        Logger::Get()->Log("[AMD-Hypervisor] - disable_hv vmmcall id %p \n", id);

        *EndVM = true;
        break;
    }
    case VMMCALL_ID::is_hv_present:
    {
        break;
    }
    default:
    {
        InjectException(vmcb_data, EXCEPTION_INVALID_OPCODE, false, 0);
        return;
    }
    }

    vmcb_data->guest_vmcb.save_state_area.Rip = vmcb_data->guest_vmcb.control_area.NRip;
}