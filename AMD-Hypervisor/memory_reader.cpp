#pragma once
#include "memory_reader.h"

namespace MemoryUtils
{
    MappingWindow page_map_view;

    void Init()
    {
        CR3 cr3;
        cr3.Flags = __readcr3();

        page_map_view.reserved_page = ExAllocatePoolZero(NonPagedPool, PAGE_SIZE, 'ENON');
        page_map_view.reserved_page_pte = (PTE_64*)Utils::GetPte(page_map_view.reserved_page, cr3.AddressOfPageDirectory << PAGE_SHIFT);
    }

    uint8_t* MapPhysicalMem(void* phys_addr)
    {
        __invlpg(page_map_view.reserved_page);

        page_map_view.reserved_page_pte->Write = true;
        page_map_view.reserved_page_pte->Present = true;
        page_map_view.reserved_page_pte->PageFrameNumber = (uintptr_t)phys_addr >> PAGE_SHIFT;

        return (uint8_t*)page_map_view.reserved_page + ((uintptr_t)phys_addr & (PAGE_SIZE - 1));
    }

    uint8_t* MapVirtualMem(void* virtual_addr, CR3 context)
    {
        auto guest_pte = Utils::GetPte(virtual_addr, context.AddressOfPageDirectory << PAGE_SHIFT);

        return MapPhysicalMem((void*)(guest_pte->PageFrameNumber << PAGE_SHIFT));
    }

    void ReadVirtualMemory(void* out_buffer, size_t length, void* virtual_addr, CR3 context)
    {
        memcpy(out_buffer, MapVirtualMem(virtual_addr, context), length);
    }

    void WriteVirtualMemory(void* in_buffer, size_t length, void* virtual_addr, CR3 context)
    {
        memcpy(MapVirtualMem(virtual_addr, context), in_buffer, length);
    }
};

