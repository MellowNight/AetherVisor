#pragma once
#include "utils.h"

class PhysMemAccess
{
    public:

    struct PageFrame
    {
        uint8_t*   window;
        PTE_64* window_pte;
    };

    PageFrame mapping_window;

    PhysMemAccess()
    {
        CR3 cr3; cr3.Flags = __readcr3();

        mapping_window.window = (uint8_t*)ExAllocatePool(NonPagedPool, PAGE_SIZE);
        mapping_window.window_pte = (PTE_64*)Utils::GetPte(mapping_window.window, cr3.AddressOfPageDirectory << PAGE_SHIFT);
    }


    void ReadVirtual(void* virtual_addr, void* buffer, int32_t size)
    {
        mapping_window.window_pte->PageFrameNumber =
            Utils::VirtualAddrToPfn((uintptr_t)virtual_addr);

        __invlpg(mapping_window.window);

        auto page_offset = (uintptr_t)virtual_addr & (PAGE_SIZE - 1);

        memcpy(buffer, mapping_window.window + page_offset, size);
    }

    void WriteVirtual(void* virtual_addr, void* buffer, int32_t size)
    {
        mapping_window.window_pte->PageFrameNumber =
            Utils::VirtualAddrToPfn((uintptr_t)virtual_addr);

        __invlpg(mapping_window.window);

        auto page_offset = (uintptr_t)virtual_addr & (PAGE_SIZE - 1);

        memcpy(mapping_window.window + page_offset, buffer, size);
    }


    template<class T>
    T ReadVirtual(void* virtual_addr)
    {
        T buffer;

        ReadVirtual(virtual_addr, &buffer, sizeof(T));

        return buffer;
    }

    template<class T>
    void WriteVirtual(void* virtual_addr, T* buffer)
    {
        WriteVirtual(virtual_addr, buffer, sizeof(T));
    }
};

