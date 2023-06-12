---
layout: doc
title: "Starboard Module Reference: memory.h"
---

Defines functions for memory allocation, alignment, copying, and comparing.

## Porters ##

All of the "Unchecked" and "Free" functions must be implemented, but they should
not be called directly. The Starboard platform wraps them with extra accounting
under certain circumstances.

## Porters and Application Developers ##

Nobody should call the "Checked", "Unchecked" or "Free" functions directly
because that evades Starboard's memory tracking. In both port implementations
and Starboard client application code, you should always call SbMemoryAllocate
and SbMemoryDeallocate rather than SbMemoryAllocateUnchecked and SbMemoryFree.

*   The "checked" functions are SbMemoryAllocateChecked(),
    SbMemoryReallocateChecked(), and SbMemoryAllocateAlignedChecked().

*   The "unchecked" functions are SbMemoryAllocateUnchecked(),
    SbMemoryReallocateUnchecked(), and SbMemoryAllocateAlignedUnchecked().

*   The "free" functions are SbMemoryFree() and SbMemoryFreeAligned().

## Enums ##

### SbMemoryMapFlags ###

The bitwise OR of these flags should be passed to SbMemoryMap to indicate how
the mapped memory can be used.

#### Values ####

*   `kSbMemoryMapProtectReserved`

    No flags set: Reserves virtual address space. SbMemoryProtect() can later
    make it accessible.
*   `kSbMemoryMapProtectRead`
*   `kSbMemoryMapProtectWrite`
*   `kSbMemoryMapProtectExec`
*   `kSbMemoryMapProtectReadWrite`

## Functions ##

### SbMemoryAllocate ###

Allocates and returns a chunk of memory of at least `size` bytes. This function
should be called from the client codebase. It is intended to be a drop-in
replacement for `malloc`.

Note that this function returns `NULL` if it is unable to allocate the memory.

`size`: The amount of memory to be allocated. If `size` is 0, the function may
return `NULL` or it may return a unique pointer value that can be passed to
SbMemoryDeallocate.

#### Declaration ####

```
void* SbMemoryAllocate(size_t size)
```

### SbMemoryAllocateAligned ###

Allocates and returns a chunk of memory of at least `size` bytes, aligned to
`alignment`. This function should be called from the client codebase. It is
meant to be a drop-in replacement for `memalign`.

The function returns `NULL` if it cannot allocate the memory. In addition, the
function's behavior is undefined if `alignment` is not a power of two.

`alignment`: The way that data is arranged and accessed in memory. The value
must be a power of two. `size`: The size of the memory to be allocated. If
`size` is `0`, the function may return `NULL` or it may return a unique aligned
pointer value that can be passed to SbMemoryDeallocateAligned.

#### Declaration ####

```
void* SbMemoryAllocateAligned(size_t alignment, size_t size)
```

### SbMemoryAllocateAlignedChecked ###

Same as SbMemoryAllocateAlignedUnchecked, but will abort() in the case of an
allocation failure.

DO NOT CALL. Call SbMemoryAllocateAligned(...) instead.

#### Declaration ####

```
void* SbMemoryAllocateAlignedChecked(size_t alignment, size_t size)
```

### SbMemoryAllocateAlignedUnchecked ###

This is the implementation of SbMemoryAllocateAligned that must be provided by
Starboard ports.

DO NOT CALL. Call SbMemoryAllocateAligned(...) instead.

#### Declaration ####

```
void* SbMemoryAllocateAlignedUnchecked(size_t alignment, size_t size)
```

### SbMemoryAllocateChecked ###

Same as SbMemoryAllocateUnchecked, but will abort() in the case of an allocation
failure.

DO NOT CALL. Call SbMemoryAllocate(...) instead.

#### Declaration ####

```
void* SbMemoryAllocateChecked(size_t size)
```

### SbMemoryAllocateNoReport ###

Same as SbMemoryAllocate() but will not report memory to the tracker. Avoid
using this unless absolutely necessary.

#### Declaration ####

```
void* SbMemoryAllocateNoReport(size_t size)
```

### SbMemoryAllocateUnchecked ###

This is the implementation of SbMemoryAllocate that must be provided by
Starboard ports.

DO NOT CALL. Call SbMemoryAllocate(...) instead.

#### Declaration ####

```
void* SbMemoryAllocateUnchecked(size_t size)
```

### SbMemoryCalloc ###

A wrapper that implements a drop-in replacement for `calloc`, which is used in
some packages.

#### Declaration ####

```
static void* SbMemoryCalloc(size_t count, size_t size)
```

### SbMemoryDeallocate ###

Frees a previously allocated chunk of memory. If `memory` is NULL, then the
operation is a no-op. This function should be called from the client codebase.
It is meant to be a drop-in replacement for `free`.

`memory`: The chunk of memory to be freed.

#### Declaration ####

```
void SbMemoryDeallocate(void *memory)
```

### SbMemoryDeallocateAligned ###

`memory`: The chunk of memory to be freed. If `memory` is NULL, then the
function is a no-op.

#### Declaration ####

```
void SbMemoryDeallocateAligned(void *memory)
```

### SbMemoryDeallocateNoReport ###

Same as SbMemoryDeallocate() but will not report memory deallocation to the
tracker. This function must be matched with SbMemoryAllocateNoReport().

#### Declaration ####

```
void SbMemoryDeallocateNoReport(void *memory)
```

### SbMemoryFlush ###

Flushes any data in the given virtual address range that is cached locally in
the current processor core to physical memory, ensuring that data and
instruction caches are cleared. This is required to be called on executable
memory that has been written to and might be executed in the future.

#### Declaration ####

```
void SbMemoryFlush(void *virtual_address, int64_t size_bytes)
```

### SbMemoryFree ###

This is the implementation of SbMemoryDeallocate that must be provided by
Starboard ports.

DO NOT CALL. Call SbMemoryDeallocate(...) instead.

#### Declaration ####

```
void SbMemoryFree(void *memory)
```

### SbMemoryFreeAligned ###

This is the implementation of SbMemoryFreeAligned that must be provided by
Starboard ports.

DO NOT CALL. Call SbMemoryDeallocateAligned(...) instead.

#### Declaration ####

```
void SbMemoryFreeAligned(void *memory)
```

### SbMemoryMap ###

Allocates `size_bytes` worth of physical memory pages and maps them into an
available virtual region. This function returns `SB_MEMORY_MAP_FAILED` on
failure. `NULL` is a valid return value.

`size_bytes`: The amount of physical memory pages to be allocated. `flags`: The
bitwise OR of the protection flags for the mapped memory as specified in
`SbMemoryMapFlags`. Allocating executable memory is not allowed and will fail.
If executable memory is needed, map non-executable memory first and then switch
access to executable using SbMemoryProtect. When kSbMemoryMapProtectReserved is
used, the address space will not be accessible and, if possible, the platform
should not count it against any memory budget. `name`: A value that appears in
the debugger on some platforms. The value can be up to 32 bytes.

#### Declaration ####

```
void* SbMemoryMap(int64_t size_bytes, int flags, const char *name)
```

### SbMemoryProtect ###

Change the protection of `size_bytes` of memory regions, starting from
`virtual_address`, to `flags`, returning `true` on success.

#### Declaration ####

```
bool SbMemoryProtect(void *virtual_address, int64_t size_bytes, int flags)
```

### SbMemoryReallocate ###

Attempts to resize `memory` to be at least `size` bytes, without touching the
contents of memory.

*   If the function cannot perform the fast resize, it allocates a new chunk of
    memory, copies the contents over, and frees the previous chunk, returning a
    pointer to the new chunk.

*   If the function cannot perform the slow resize, it returns `NULL`, leaving
    the given memory chunk unchanged.

This function should be called from the client codebase. It is meant to be a
drop-in replacement for `realloc`.

`memory`: The chunk of memory to be resized. `memory` may be NULL, in which case
it behaves exactly like SbMemoryAllocateUnchecked. `size`: The size to which
`memory` will be resized. If `size` is `0`, the function may return `NULL` or it
may return a unique pointer value that can be passed to SbMemoryDeallocate.

#### Declaration ####

```
void* SbMemoryReallocate(void *memory, size_t size)
```

### SbMemoryReallocateChecked ###

Same as SbMemoryReallocateUnchecked, but will abort() in the case of an
allocation failure.

DO NOT CALL. Call SbMemoryReallocate(...) instead.

#### Declaration ####

```
void* SbMemoryReallocateChecked(void *memory, size_t size)
```

### SbMemoryReallocateUnchecked ###

This is the implementation of SbMemoryReallocate that must be provided by
Starboard ports.

DO NOT CALL. Call SbMemoryReallocate(...) instead.

#### Declaration ####

```
void* SbMemoryReallocateUnchecked(void *memory, size_t size)
```

### SbMemoryUnmap ###

Unmap `size_bytes` of physical pages starting from `virtual_address`, returning
`true` on success. After this function completes, [virtual_address,
virtual_address + size_bytes) will not be read/writable. This function can unmap
multiple contiguous regions that were mapped with separate calls to
SbMemoryMap(). For example, if one call to `SbMemoryMap(0x1000)` returns
`(void*)0xA000`, and another call to `SbMemoryMap(0x1000)` returns
`(void*)0xB000`, `SbMemoryUnmap(0xA000, 0x2000)` should free both regions.

#### Declaration ####

```
bool SbMemoryUnmap(void *virtual_address, int64_t size_bytes)
```
