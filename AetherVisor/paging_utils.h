#pragma once
#include "utils.h"

class PhysMemAccess
{
    public:

    struct PageFrame
    {
        void*   window;
        PTE_64* window_pte;
    };

    PageFrame mapping_window;

    PhysMemAccess()
    {
        CR3 cr3; cr3.Flags = __readcr3();

        mapping_window.window = ExAllocatePool(NonPagedPool, PAGE_SIZE);
        mapping_window.window_pte = (PTE_64*)Memory::GetPte(mapping_window.eservedPage, cr3);
    }

    template<class T>
    T ReadVirtual(void* virtual_addr)
    {
        mapping_window.window_pte->PageFrameNumber = Utils::VirtualAddrToPfn(virtual_addr);
        return *(T*)virtual_addr;
    }
    
    template<class T>
    void WriteVirtual(T* buffer, size_t size)
    {
        mapping_window.window_pte->PageFrameNumber = Utils::VirtualAddrToPfn(virtual_addr);
        memcpy(mapping_window.window, buffer, size)
    }
};

