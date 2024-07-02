Project: /youtube/cobalt/_project.yaml
Book: /youtube/cobalt/_book.yaml

# Starboard Module Reference: `memory.h`

Defines functions for memory allocation, alignment, copying, and comparing.

## Porters

All of the "Unchecked" and "Free" functions must be implemented, but they should
not be called directly. The Starboard platform wraps them with extra accounting
under certain circumstances.

## Porters and Application Developers

Nobody should call the "Checked", "Unchecked" or "Free" functions directly
because that evades Starboard's memory tracking. In both port implementations
and Starboard client application code, you should always call SbMemoryAllocate
and SbMemoryDeallocate rather than SbMemoryAllocateUnchecked and SbMemoryFree.

*   The "checked" functions are SbMemoryAllocateChecked(),
    SbMemoryReallocateChecked(), and SbMemoryAllocateAlignedChecked().

*   The "unchecked" functions are SbMemoryAllocateUnchecked(),
    SbMemoryReallocateUnchecked(), and SbMemoryAllocateAlignedUnchecked().

*   The "free" functions are SbMemoryFree() and SbMemoryFreeAligned().

## Enums

### SbMemoryMapFlags

TODO: Remove the definition once the memory_mapped_file.h extension is
deprecated. The bitwise OR of these flags should be passed to SbMemoryMap to
indicate how the mapped memory can be used.

#### Values

*   `kSbMemoryMapProtectReserved`

    No flags set: Reserves virtual address space. SbMemoryProtect() can later
    make it accessible.
*   `kSbMemoryMapProtectRead`
*   `kSbMemoryMapProtectWrite`
*   `kSbMemoryMapProtectExec`
*   `kSbMemoryMapProtectReadWrite`
