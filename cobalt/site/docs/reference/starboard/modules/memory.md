---
layout: doc
title: "Starboard Module Reference: memory.h"
---

Defines functions for memory allocation, alignment, copying, and comparing.<br>
#### Porters


All of the "Unchecked" and "Free" functions must be implemented, but they
should not be called directly. The Starboard platform wraps them with extra
accounting under certain circumstances.<br>
#### Porters and Application Developers


Nobody should call the "Checked", "Unchecked" or "Free" functions directly
because that evades Starboard's memory tracking. In both port
implementations and Starboard client application code, you should always
call SbMemoryAllocate and SbMemoryDeallocate rather than
SbMemoryAllocateUnchecked and SbMemoryFree.<br>
<ul><li>The "checked" functions are SbMemoryAllocateChecked(),
SbMemoryReallocateChecked(), and SbMemoryAllocateAlignedChecked().
</li><li>The "unchecked" functions are SbMemoryAllocateUnchecked(),
SbMemoryReallocateUnchecked(), and SbMemoryAllocateAlignedUnchecked().
</li><li>The "free" functions are SbMemoryFree() and SbMemoryFreeAligned().</li></ul>

## Enums

### SbMemoryMapFlags

The bitwise OR of these flags should be passed to SbMemoryMap to indicate
how the mapped memory can be used.

**Values**

*   `kSbMemoryMapProtectRead` - Mapped memory can be read.
*   `kSbMemoryMapProtectWrite` - Mapped memory can be written to.
*   `kSbMemoryMapProtectExec` - Mapped memory can be executed.
*   `kSbMemoryMapProtectRead`

## Macros

<div id="macro-documentation-section">

<h3 id="sb_memory_map_failed" class="small-h3">SB_MEMORY_MAP_FAILED</h3>

</div>

## Functions

### SbAbortIfAllocationFailed

**Declaration**

```
static SB_C_FORCE_INLINE void SbAbortIfAllocationFailed(size_t requested_bytes,
                                                        void* address) {
  if (SB_UNLIKELY(requested_bytes > 0 && address == NULL)) {
    // Will abort the program if no debugger is attached.
    SbSystemBreakIntoDebugger();
  }
}
```

**Parameters**



<table class="responsive">
  <tr><th colspan="2">Parameters</th></tr>
  <tr>
    <td><code>size_t</code><br>
        <code>requested_bytes</code></td>
    <td> </td>
  </tr>
  <tr>
    <td><code>void*</code><br>
        <code>address</code></td>
    <td> </td>
  </tr>
</table>

### SbMemoryAlignToPageSize

**Description**

Rounds `size` up to SB_MEMORY_PAGE_SIZE.

**Declaration**

```
static SB_C_FORCE_INLINE size_t SbMemoryAlignToPageSize(size_t size) {
  return (size + SB_MEMORY_PAGE_SIZE - 1) & ~(SB_MEMORY_PAGE_SIZE - 1);
}
```

**Parameters**



<table class="responsive">
  <tr><th colspan="2">Parameters</th></tr>
  <tr>
    <td><code>size_t</code><br>
        <code>size</code></td>
    <td> </td>
  </tr>
</table>

### SbMemoryAllocate

**Description**

Allocates and returns a chunk of memory of at least `size` bytes. This
function should be called from the client codebase. It is intended to be a
drop-in replacement for `malloc`.<br>
Note that this function returns `NULL` if it is unable to allocate the
memory.

**Declaration**

```
SB_EXPORT void* SbMemoryAllocate(size_t size);
```

**Parameters**



<table class="responsive">
  <tr><th colspan="2">Parameters</th></tr>
  <tr>
    <td><code>size_t</code><br>        <code>size</code></td>
    <td>The amount of memory to be allocated. If <code>size</code> is 0, the function
may return <code>NULL</code> or it may return a unique pointer value that can be
passed to <code><a href="#sbmemorydeallocate">SbMemoryDeallocate</a></code>.</td>
  </tr>
</table>

### SbMemoryAllocateAligned

**Description**

Allocates and returns a chunk of memory of at least `size` bytes, aligned to
`alignment`. This function should be called from the client codebase. It is
meant to be a drop-in replacement for `memalign`.<br>
The function returns `NULL` if it cannot allocate the memory. In addition,
the function's behavior is undefined if `alignment` is not a power of two.

**Declaration**

```
SB_EXPORT void* SbMemoryAllocateAligned(size_t alignment, size_t size);
```

**Parameters**



<table class="responsive">
  <tr><th colspan="2">Parameters</th></tr>
  <tr>
    <td><code>size_t</code><br>        <code>alignment</code></td>
    <td>The way that data is arranged and accessed in memory. The value
must be a power of two.</td>
  </tr>
  <tr>
    <td><code>size_t</code><br>        <code>size</code></td>
    <td>The size of the memory to be allocated. If <code>size</code> is <code>0</code>, the
function may return <code>NULL</code> or it may return a unique aligned pointer value
that can be passed to <code><a href="#sbmemorydeallocate">SbMemoryDeallocate</a></code>Aligned.</td>
  </tr>
</table>

### SbMemoryAllocateAlignedChecked

**Description**

Same as <code><a href="#sbmemoryallocate">SbMemoryAllocate</a></code>AlignedUnchecked, but will abort() in the case of an
allocation failure.<br>
DO NOT CALL. Call <code><a href="#sbmemoryallocate">SbMemoryAllocate</a></code>Aligned(...) instead.

**Declaration**

```
    SB_EXPORT void* SbMemoryAllocateAlignedChecked(size_t alignment,
                                                   size_t size));
```

**Parameters**



<table class="responsive">
  <tr><th colspan="2">Parameters</th></tr>
  <tr>
    <td><code>size_t</code><br>
        <code>alignment</code></td>
    <td> </td>
  </tr>
  <tr>
    <td><code>size_t</code><br>
        <code>size</code></td>
    <td> </td>
  </tr>
</table>

### SbMemoryAllocateAlignedUnchecked

**Description**

This is the implementation of <code><a href="#sbmemoryallocate">SbMemoryAllocate</a></code>Aligned that must be
provided by Starboard ports.<br>
DO NOT CALL. Call <code><a href="#sbmemoryallocate">SbMemoryAllocate</a></code>Aligned(...) instead.

**Declaration and definitions**

<div class="mdl-tabs mdl-js-tabs mdl-js-ripple-effect">
  <div class="mdl-tabs__tab-bar">
    <a href="#SbMemoryAllocateAlignedUnchecked-declaration" class="mdl-tabs__tab is-active">Declaration</a>
    <a href="#SbMemoryAllocateAlignedUnchecked-stub" class="mdl-tabs__tab">stub</a>
  </div>
  <div class="mdl-tabs__panel is-active" id="SbMemoryAllocateAlignedUnchecked-declaration">
<pre>
    SB_EXPORT void* SbMemoryAllocateAlignedUnchecked(size_t alignment,
                                                     size_t size));
</pre>
</div>
  <div class="mdl-tabs__panel" id="SbMemoryAllocateAlignedUnchecked-stub">

```
#include "starboard/memory.h"

void* SbMemoryAllocateAlignedUnchecked(size_t /*alignment*/, size_t /*size*/) {
  return NULL;
}
```

  </div>
</div>

**Parameters**



<table class="responsive">
  <tr><th colspan="2">Parameters</th></tr>
  <tr>
    <td><code>size_t</code><br>
        <code>alignment</code></td>
    <td> </td>
  </tr>
  <tr>
    <td><code>size_t</code><br>
        <code>size</code></td>
    <td> </td>
  </tr>
</table>

### SbMemoryAllocateChecked

**Description**

Same as <code><a href="#sbmemoryallocate">SbMemoryAllocate</a></code>Unchecked, but will abort() in the case of an
allocation failure.<br>
DO NOT CALL. Call <code><a href="#sbmemoryallocate">SbMemoryAllocate</a></code>(...) instead.

**Declaration**

```
    SB_EXPORT void* SbMemoryAllocateChecked(size_t size));
```

**Parameters**



<table class="responsive">
  <tr><th colspan="2">Parameters</th></tr>
  <tr>
    <td><code>size_t</code><br>
        <code>size</code></td>
    <td> </td>
  </tr>
</table>

### SbMemoryAllocateNoReport

**Description**

Same as <code><a href="#sbmemoryallocate">SbMemoryAllocate()</a></code> but will not report memory to the tracker. Avoid
using this unless absolutely necessary.

**Declaration**

```
SB_EXPORT void* SbMemoryAllocateNoReport(size_t size);
```

**Parameters**



<table class="responsive">
  <tr><th colspan="2">Parameters</th></tr>
  <tr>
    <td><code>size_t</code><br>
        <code>size</code></td>
    <td> </td>
  </tr>
</table>

### SbMemoryAllocateUnchecked

**Description**

This is the implementation of <code><a href="#sbmemoryallocate">SbMemoryAllocate</a></code> that must be
provided by Starboard ports.<br>
DO NOT CALL. Call <code><a href="#sbmemoryallocate">SbMemoryAllocate</a></code>(...) instead.

**Declaration and definitions**

<div class="mdl-tabs mdl-js-tabs mdl-js-ripple-effect">
  <div class="mdl-tabs__tab-bar">
    <a href="#SbMemoryAllocateUnchecked-declaration" class="mdl-tabs__tab is-active">Declaration</a>
    <a href="#SbMemoryAllocateUnchecked-stub" class="mdl-tabs__tab">stub</a>
  </div>
  <div class="mdl-tabs__panel is-active" id="SbMemoryAllocateUnchecked-declaration">
<pre>
    SB_EXPORT void* SbMemoryAllocateUnchecked(size_t size));
</pre>
</div>
  <div class="mdl-tabs__panel" id="SbMemoryAllocateUnchecked-stub">

```
#include "starboard/memory.h"

void* SbMemoryAllocateUnchecked(size_t /*size*/) {
  return NULL;
}
```

  </div>
</div>

**Parameters**



<table class="responsive">
  <tr><th colspan="2">Parameters</th></tr>
  <tr>
    <td><code>size_t</code><br>
        <code>size</code></td>
    <td> </td>
  </tr>
</table>

### SbMemoryCalloc

**Description**

A wrapper that implements a drop-in replacement for `calloc`, which is used
in some packages.

**Declaration**

```
static SB_C_INLINE void* SbMemoryCalloc(size_t count, size_t size) {
  size_t total = count * size;
  void* result = SbMemoryAllocate(total);
  if (result) {
    SbMemorySet(result, 0, total);
  }
  return result;
}
```

**Parameters**



<table class="responsive">
  <tr><th colspan="2">Parameters</th></tr>
  <tr>
    <td><code>size_t</code><br>
        <code>count</code></td>
    <td> </td>
  </tr>
  <tr>
    <td><code>size_t</code><br>
        <code>size</code></td>
    <td> </td>
  </tr>
</table>

### SbMemoryCompare

**Description**

Compares the contents of the first `count` bytes of `buffer1` and `buffer2`.
This function returns:
<ul><li>`-1` if `buffer1` is "less-than" `buffer2`
</li><li>`0` if `buffer1` and `buffer2` are equal
</li><li>`1` if `buffer1` is "greater-than" `buffer2`.</li></ul>
This function is meant to be a drop-in replacement for `memcmp`.

**Declaration and definitions**

<div class="mdl-tabs mdl-js-tabs mdl-js-ripple-effect">
  <div class="mdl-tabs__tab-bar">
    <a href="#SbMemoryCompare-declaration" class="mdl-tabs__tab is-active">Declaration</a>
    <a href="#SbMemoryCompare-stub" class="mdl-tabs__tab">stub</a>
  </div>
  <div class="mdl-tabs__panel is-active" id="SbMemoryCompare-declaration">
<pre>
SB_EXPORT int SbMemoryCompare(const void* buffer1,
                              const void* buffer2,
                              size_t count);
</pre>
</div>
  <div class="mdl-tabs__panel" id="SbMemoryCompare-stub">

```
#include "starboard/memory.h"

int SbMemoryCompare(const void* /*buffer1*/, const void* /*buffer2*/,
                    size_t /*count*/) {
  return 0;
}
```

  </div>
</div>

**Parameters**



<table class="responsive">
  <tr><th colspan="2">Parameters</th></tr>
  <tr>
    <td><code>const void*</code><br>        <code>buffer1</code></td>
    <td>The first buffer to be compared.</td>
  </tr>
  <tr>
    <td><code>const void*</code><br>        <code>buffer2</code></td>
    <td>The second buffer to be compared.</td>
  </tr>
  <tr>
    <td><code>size_t</code><br>        <code>count</code></td>
    <td>The number of bytes to be compared.</td>
  </tr>
</table>

### SbMemoryCopy

**Description**

Copies `count` sequential bytes from `source` to `destination`, without
support for the `source` and `destination` regions overlapping. This
function is meant to be a drop-in replacement for `memcpy`.<br>
The function's behavior is undefined if `destination` or `source` are NULL,
and the function is a no-op if `count` is 0. The return value is
`destination`.

**Declaration and definitions**

<div class="mdl-tabs mdl-js-tabs mdl-js-ripple-effect">
  <div class="mdl-tabs__tab-bar">
    <a href="#SbMemoryCopy-declaration" class="mdl-tabs__tab is-active">Declaration</a>
    <a href="#SbMemoryCopy-stub" class="mdl-tabs__tab">stub</a>
  </div>
  <div class="mdl-tabs__panel is-active" id="SbMemoryCopy-declaration">
<pre>
SB_EXPORT void* SbMemoryCopy(void* destination,
                             const void* source,
                             size_t count);
</pre>
</div>
  <div class="mdl-tabs__panel" id="SbMemoryCopy-stub">

```
#include "starboard/memory.h"

void* SbMemoryCopy(void* /*destination*/, const void* /*source*/,
                   size_t /*count*/) {
  return NULL;
}
```

  </div>
</div>

**Parameters**



<table class="responsive">
  <tr><th colspan="2">Parameters</th></tr>
  <tr>
    <td><code>void*</code><br>        <code>destination</code></td>
    <td>The destination of the copied memory.</td>
  </tr>
  <tr>
    <td><code>const void*</code><br>        <code>source</code></td>
    <td>The source of the copied memory.</td>
  </tr>
  <tr>
    <td><code>size_t</code><br>        <code>count</code></td>
    <td>The number of sequential bytes to be copied.</td>
  </tr>
</table>

### SbMemoryDeallocate

**Description**

Frees a previously allocated chunk of memory. If `memory` is NULL, then the
operation is a no-op. This function should be called from the client
codebase. It is meant to be a drop-in replacement for `free`.

**Declaration**

```
SB_EXPORT void SbMemoryDeallocate(void* memory);
```

**Parameters**



<table class="responsive">
  <tr><th colspan="2">Parameters</th></tr>
  <tr>
    <td><code>void*</code><br>        <code>memory</code></td>
    <td>The chunk of memory to be freed.</td>
  </tr>
</table>

### SbMemoryDeallocateAligned

**Declaration**

```
SB_EXPORT void SbMemoryDeallocateAligned(void* memory);
```

**Parameters**



<table class="responsive">
  <tr><th colspan="2">Parameters</th></tr>
  <tr>
    <td><code>void*</code><br>        <code>memory</code></td>
    <td>The chunk of memory to be freed. If <code>memory</code> is NULL, then the
function is a no-op.</td>
  </tr>
</table>

### SbMemoryDeallocateNoReport

**Description**

Same as <code><a href="#sbmemorydeallocate">SbMemoryDeallocate()</a></code> but will not report memory deallocation to the
tracker. This function must be matched with <code><a href="#sbmemoryallocate">SbMemoryAllocate</a></code>NoReport().

**Declaration**

```
SB_EXPORT void SbMemoryDeallocateNoReport(void* memory);
```

**Parameters**



<table class="responsive">
  <tr><th colspan="2">Parameters</th></tr>
  <tr>
    <td><code>void*</code><br>
        <code>memory</code></td>
    <td> </td>
  </tr>
</table>

### SbMemoryFindByte

**Description**

Finds the lower 8-bits of `value` in the first `count` bytes of `buffer`
and returns either a pointer to the first found occurrence or `NULL` if
the value is not found. This function is meant to be a drop-in replacement
for `memchr`.

**Declaration and definitions**

<div class="mdl-tabs mdl-js-tabs mdl-js-ripple-effect">
  <div class="mdl-tabs__tab-bar">
    <a href="#SbMemoryFindByte-declaration" class="mdl-tabs__tab is-active">Declaration</a>
    <a href="#SbMemoryFindByte-stub" class="mdl-tabs__tab">stub</a>
  </div>
  <div class="mdl-tabs__panel is-active" id="SbMemoryFindByte-declaration">
<pre>
SB_EXPORT const void* SbMemoryFindByte(const void* buffer,
                                       int value,
                                       size_t count);
</pre>
</div>
  <div class="mdl-tabs__panel" id="SbMemoryFindByte-stub">

```
#include "starboard/memory.h"

const void* SbMemoryFindByte(const void* /*buffer*/, int /*value*/,
                             size_t /*count*/) {
  return 0;
}
```

  </div>
</div>

**Parameters**



<table class="responsive">
  <tr><th colspan="2">Parameters</th></tr>
  <tr>
    <td><code>const void*</code><br>
        <code>buffer</code></td>
    <td> </td>
  </tr>
  <tr>
    <td><code>int</code><br>
        <code>value</code></td>
    <td> </td>
  </tr>
  <tr>
    <td><code>size_t</code><br>
        <code>count</code></td>
    <td> </td>
  </tr>
</table>

### SbMemoryFlush

**Description**

Flushes any data in the given virtual address range that is cached locally in
the current processor core to physical memory, ensuring that data and
instruction caches are cleared. This is required to be called on executable
memory that has been written to and might be executed in the future.

**Declaration and definitions**

<div class="mdl-tabs mdl-js-tabs mdl-js-ripple-effect">
  <div class="mdl-tabs__tab-bar">
    <a href="#SbMemoryFlush-declaration" class="mdl-tabs__tab is-active">Declaration</a>
    <a href="#SbMemoryFlush-stub" class="mdl-tabs__tab">stub</a>
  </div>
  <div class="mdl-tabs__panel is-active" id="SbMemoryFlush-declaration">
<pre>
SB_EXPORT void SbMemoryFlush(void* virtual_address, int64_t size_bytes);
</pre>
</div>
  <div class="mdl-tabs__panel" id="SbMemoryFlush-stub">

```
#include "starboard/memory.h"

void SbMemoryFlush(void* /*virtual_address*/, int64_t /*size_bytes*/) {}
```

  </div>
</div>

**Parameters**



<table class="responsive">
  <tr><th colspan="2">Parameters</th></tr>
  <tr>
    <td><code>void*</code><br>
        <code>virtual_address</code></td>
    <td> </td>
  </tr>
  <tr>
    <td><code>int64_t</code><br>
        <code>size_bytes</code></td>
    <td> </td>
  </tr>
</table>

### SbMemoryFree

**Description**

This is the implementation of <code><a href="#sbmemorydeallocate">SbMemoryDeallocate</a></code> that must be provided by
Starboard ports.<br>
DO NOT CALL. Call <code><a href="#sbmemorydeallocate">SbMemoryDeallocate</a></code>(...) instead.

**Declaration and definitions**

<div class="mdl-tabs mdl-js-tabs mdl-js-ripple-effect">
  <div class="mdl-tabs__tab-bar">
    <a href="#SbMemoryFree-declaration" class="mdl-tabs__tab is-active">Declaration</a>
    <a href="#SbMemoryFree-stub" class="mdl-tabs__tab">stub</a>
  </div>
  <div class="mdl-tabs__panel is-active" id="SbMemoryFree-declaration">
<pre>
    SB_EXPORT void SbMemoryFree(void* memory));
</pre>
</div>
  <div class="mdl-tabs__panel" id="SbMemoryFree-stub">

```
#include "starboard/memory.h"

void SbMemoryFree(void* /*memory*/) {
}
```

  </div>
</div>

**Parameters**



<table class="responsive">
  <tr><th colspan="2">Parameters</th></tr>
  <tr>
    <td><code>void*</code><br>
        <code>memory</code></td>
    <td> </td>
  </tr>
</table>

### SbMemoryFreeAligned

**Description**

This is the implementation of <code>SbMemoryFreeAligned</code> that must be provided by
Starboard ports.<br>
DO NOT CALL. Call <code><a href="#sbmemorydeallocate">SbMemoryDeallocate</a></code>Aligned(...) instead.

**Declaration and definitions**

<div class="mdl-tabs mdl-js-tabs mdl-js-ripple-effect">
  <div class="mdl-tabs__tab-bar">
    <a href="#SbMemoryFreeAligned-declaration" class="mdl-tabs__tab is-active">Declaration</a>
    <a href="#SbMemoryFreeAligned-stub" class="mdl-tabs__tab">stub</a>
  </div>
  <div class="mdl-tabs__panel is-active" id="SbMemoryFreeAligned-declaration">
<pre>
    SB_EXPORT void SbMemoryFreeAligned(void* memory));
</pre>
</div>
  <div class="mdl-tabs__panel" id="SbMemoryFreeAligned-stub">

```
#include "starboard/memory.h"

void SbMemoryFreeAligned(void* /*memory*/) {
}
```

  </div>
</div>

**Parameters**



<table class="responsive">
  <tr><th colspan="2">Parameters</th></tr>
  <tr>
    <td><code>void*</code><br>
        <code>memory</code></td>
    <td> </td>
  </tr>
</table>

### SbMemoryGetStackBounds

**Description**

Gets the stack bounds for the current thread.

**Declaration and definitions**

<div class="mdl-tabs mdl-js-tabs mdl-js-ripple-effect">
  <div class="mdl-tabs__tab-bar">
    <a href="#SbMemoryGetStackBounds-declaration" class="mdl-tabs__tab is-active">Declaration</a>
    <a href="#SbMemoryGetStackBounds-stub" class="mdl-tabs__tab">stub</a>
    <a href="#SbMemoryGetStackBounds-bsd" class="mdl-tabs__tab">bsd</a>
    <a href="#SbMemoryGetStackBounds-linux" class="mdl-tabs__tab">linux</a>
  </div>
  <div class="mdl-tabs__panel is-active" id="SbMemoryGetStackBounds-declaration">
<pre>
SB_EXPORT void SbMemoryGetStackBounds(void** out_high, void** out_low);
</pre>
</div>
  <div class="mdl-tabs__panel" id="SbMemoryGetStackBounds-stub">

```
#include "starboard/memory.h"

void SbMemoryGetStackBounds(void** /*out_high*/, void** /*out_low*/) {
}
```

  </div>
  <div class="mdl-tabs__panel" id="SbMemoryGetStackBounds-bsd">

```
#include "starboard/memory.h"

#include <pthread.h>
#include <pthread_np.h>

void SbMemoryGetStackBounds(void** out_high, void** out_low) {
  pthread_attr_t attr;
  pthread_attr_init(&attr);
  pthread_attr_get_np(pthread_self(), &attr);

  void* stack_address;
  size_t stack_size;
  pthread_attr_getstack(&attr, &stack_address, &stack_size);
  *out_high = static_cast<uint8_t*>(stack_address) + stack_size;
  *out_low = stack_address;

  pthread_attr_destroy(&attr);
}
```

  </div>
  <div class="mdl-tabs__panel" id="SbMemoryGetStackBounds-linux">

```
#include "starboard/memory.h"

#include <pthread.h>

#include "starboard/log.h"

void SbMemoryGetStackBounds(void** out_high, void** out_low) {
  void* stackBase = 0;
  size_t stackSize = 0;

  pthread_t thread = pthread_self();
  pthread_attr_t sattr;
  pthread_attr_init(&sattr);
  pthread_getattr_np(thread, &sattr);
  int rc = pthread_attr_getstack(&sattr, &stackBase, &stackSize);
  SB_DCHECK(rc == 0);
  SB_DCHECK(stackBase);
  pthread_attr_destroy(&sattr);
  *out_high = static_cast<char*>(stackBase) + stackSize;
  *out_low = stackBase;
}
```

  </div>
</div>

**Parameters**



<table class="responsive">
  <tr><th colspan="2">Parameters</th></tr>
  <tr>
    <td><code>void**</code><br>        <code>out_high</code></td>
    <td>The highest addressable byte + 1 for the current thread.</td>
  </tr>
  <tr>
    <td><code>void**</code><br>        <code>out_low</code></td>
    <td>The lowest addressable byte for the current thread.</td>
  </tr>
</table>

### SbMemoryIsAligned

**Description**

Checks whether `memory` is aligned to `alignment` bytes.

**Declaration**

```
static SB_C_FORCE_INLINE bool SbMemoryIsAligned(const void* memory,
                                                size_t alignment) {
  return ((uintptr_t)memory) % alignment == 0;
}
```

**Parameters**



<table class="responsive">
  <tr><th colspan="2">Parameters</th></tr>
  <tr>
    <td><code>const void*</code><br>
        <code>memory</code></td>
    <td> </td>
  </tr>
  <tr>
    <td><code>size_t</code><br>
        <code>alignment</code></td>
    <td> </td>
  </tr>
</table>

### SbMemoryIsZero

**Description**

Returns true if the first `count` bytes of `buffer` are set to zero.

**Declaration**

```
static SB_C_INLINE bool SbMemoryIsZero(const void* buffer, size_t count) {
  if (count == 0) {
    return true;
  }
  const char* char_buffer = (const char*)(buffer);
  return char_buffer[0] == 0 &&
         SbMemoryCompare(char_buffer, char_buffer + 1, count - 1) == 0;
}
```

**Parameters**



<table class="responsive">
  <tr><th colspan="2">Parameters</th></tr>
  <tr>
    <td><code>const void*</code><br>
        <code>buffer</code></td>
    <td> </td>
  </tr>
  <tr>
    <td><code>size_t</code><br>
        <code>count</code></td>
    <td> </td>
  </tr>
</table>

### SbMemoryMap

**Description**

Allocates `size_bytes` worth of physical memory pages and maps them into an
available virtual region. This function returns `SB_MEMORY_MAP_FAILED` on
failure. `NULL` is a valid return value.

**Declaration and definitions**

<div class="mdl-tabs mdl-js-tabs mdl-js-ripple-effect">
  <div class="mdl-tabs__tab-bar">
    <a href="#SbMemoryMap-declaration" class="mdl-tabs__tab is-active">Declaration</a>
    <a href="#SbMemoryMap-stub" class="mdl-tabs__tab">stub</a>
  </div>
  <div class="mdl-tabs__panel is-active" id="SbMemoryMap-declaration">
<pre>
SB_EXPORT void* SbMemoryMap(int64_t size_bytes, int flags, const char* name);
</pre>
</div>
  <div class="mdl-tabs__panel" id="SbMemoryMap-stub">

```
#include "starboard/memory.h"

void* SbMemoryMap(int64_t /*size_bytes*/, int /*flags*/, const char* /*name*/) {
  return NULL;
}
```

  </div>
</div>

**Parameters**



<table class="responsive">
  <tr><th colspan="2">Parameters</th></tr>
  <tr>
    <td><code>int64_t</code><br>        <code>size_bytes</code></td>
    <td>The amount of physical memory pages to be allocated.</td>
  </tr>
  <tr>
    <td><code>int</code><br>        <code>flags</code></td>
    <td>The bitwise OR of the protection flags for the mapped memory
as specified in <code>SbMemoryMapFlags</code>.</td>
  </tr>
  <tr>
    <td><code>const char*</code><br>        <code>name</code></td>
    <td>A value that appears in the debugger on some platforms. The value
can be up to 32 bytes.</td>
  </tr>
</table>

### SbMemoryMove

**Description**

Copies `count` sequential bytes from `source` to `destination`, with support
for the `source` and `destination` regions overlapping. This function is
meant to be a drop-in replacement for `memmove`.<br>
The function's behavior is undefined if `destination` or `source` are NULL,
and the function is a no-op if `count` is 0. The return value is
`destination`.

**Declaration and definitions**

<div class="mdl-tabs mdl-js-tabs mdl-js-ripple-effect">
  <div class="mdl-tabs__tab-bar">
    <a href="#SbMemoryMove-declaration" class="mdl-tabs__tab is-active">Declaration</a>
    <a href="#SbMemoryMove-stub" class="mdl-tabs__tab">stub</a>
  </div>
  <div class="mdl-tabs__panel is-active" id="SbMemoryMove-declaration">
<pre>
SB_EXPORT void* SbMemoryMove(void* destination,
                             const void* source,
                             size_t count);
</pre>
</div>
  <div class="mdl-tabs__panel" id="SbMemoryMove-stub">

```
#include "starboard/memory.h"

void* SbMemoryMove(void* /*destination*/, const void* /*source*/,
                   size_t /*count*/) {
  return NULL;
}
```

  </div>
</div>

**Parameters**



<table class="responsive">
  <tr><th colspan="2">Parameters</th></tr>
  <tr>
    <td><code>void*</code><br>        <code>destination</code></td>
    <td>The destination of the copied memory.</td>
  </tr>
  <tr>
    <td><code>const void*</code><br>        <code>source</code></td>
    <td>The source of the copied memory.</td>
  </tr>
  <tr>
    <td><code>size_t</code><br>        <code>count</code></td>
    <td>The number of sequential bytes to be copied.</td>
  </tr>
</table>

### SbMemoryReallocate

**Description**

Attempts to resize `memory` to be at least `size` bytes, without touching
the contents of memory.
<ul><li>If the function cannot perform the fast resize, it allocates a new chunk
of memory, copies the contents over, and frees the previous chunk,
returning a pointer to the new chunk.
</li><li>If the function cannot perform the slow resize, it returns `NULL`,
leaving the given memory chunk unchanged.</li></ul>
This function should be called from the client codebase. It is meant to be a
drop-in replacement for `realloc`.

**Declaration**

```
SB_EXPORT void* SbMemoryReallocate(void* memory, size_t size);
```

**Parameters**



<table class="responsive">
  <tr><th colspan="2">Parameters</th></tr>
  <tr>
    <td><code>void*</code><br>        <code>memory</code></td>
    <td>The chunk of memory to be resized. <code>memory</code> may be NULL, in which
case it behaves exactly like <code><a href="#sbmemoryallocate">SbMemoryAllocate</a></code>Unchecked.</td>
  </tr>
  <tr>
    <td><code>size_t</code><br>        <code>size</code></td>
    <td>The size to which <code>memory</code> will be resized. If <code>size</code> is <code>0</code>,
the function may return <code>NULL</code> or it may return a unique pointer value
that can be passed to <code><a href="#sbmemorydeallocate">SbMemoryDeallocate</a></code>.</td>
  </tr>
</table>

### SbMemoryReallocateChecked

**Description**

Same as <code><a href="#sbmemoryreallocate">SbMemoryReallocate</a></code>Unchecked, but will abort() in the case of an
allocation failure.<br>
DO NOT CALL. Call <code><a href="#sbmemoryreallocate">SbMemoryReallocate</a></code>(...) instead.

**Declaration**

```
    SB_EXPORT void* SbMemoryReallocateChecked(void* memory, size_t size));
```

**Parameters**



<table class="responsive">
  <tr><th colspan="2">Parameters</th></tr>
  <tr>
    <td><code>void*</code><br>
        <code>memory</code></td>
    <td> </td>
  </tr>
  <tr>
    <td><code>size_t</code><br>
        <code>size</code></td>
    <td> </td>
  </tr>
</table>

### SbMemoryReallocateUnchecked

**Description**

This is the implementation of <code><a href="#sbmemoryreallocate">SbMemoryReallocate</a></code> that must be
provided by Starboard ports.<br>
DO NOT CALL. Call <code><a href="#sbmemoryreallocate">SbMemoryReallocate</a></code>(...) instead.

**Declaration and definitions**

<div class="mdl-tabs mdl-js-tabs mdl-js-ripple-effect">
  <div class="mdl-tabs__tab-bar">
    <a href="#SbMemoryReallocateUnchecked-declaration" class="mdl-tabs__tab is-active">Declaration</a>
    <a href="#SbMemoryReallocateUnchecked-stub" class="mdl-tabs__tab">stub</a>
  </div>
  <div class="mdl-tabs__panel is-active" id="SbMemoryReallocateUnchecked-declaration">
<pre>
    SB_EXPORT void* SbMemoryReallocateUnchecked(void* memory, size_t size));
</pre>
</div>
  <div class="mdl-tabs__panel" id="SbMemoryReallocateUnchecked-stub">

```
#include "starboard/memory.h"

void* SbMemoryReallocateUnchecked(void* /*memory*/, size_t /*size*/) {
  return NULL;
}
```

  </div>
</div>

**Parameters**



<table class="responsive">
  <tr><th colspan="2">Parameters</th></tr>
  <tr>
    <td><code>void*</code><br>
        <code>memory</code></td>
    <td> </td>
  </tr>
  <tr>
    <td><code>size_t</code><br>
        <code>size</code></td>
    <td> </td>
  </tr>
</table>

### SbMemorySet

**Description**

Fills `count` sequential bytes starting at `destination`, with the unsigned
char coercion of `byte_value`. This function is meant to be a drop-in
replacement for `memset`.<br>
The function's behavior is undefined if `destination` is NULL, and the
function is a no-op if `count` is 0. The return value is `destination`.

**Declaration and definitions**

<div class="mdl-tabs mdl-js-tabs mdl-js-ripple-effect">
  <div class="mdl-tabs__tab-bar">
    <a href="#SbMemorySet-declaration" class="mdl-tabs__tab is-active">Declaration</a>
    <a href="#SbMemorySet-stub" class="mdl-tabs__tab">stub</a>
  </div>
  <div class="mdl-tabs__panel is-active" id="SbMemorySet-declaration">
<pre>
SB_EXPORT void* SbMemorySet(void* destination, int byte_value, size_t count);
</pre>
</div>
  <div class="mdl-tabs__panel" id="SbMemorySet-stub">

```
#include "starboard/memory.h"

void* SbMemorySet(void* /*destination*/, int /*byte_value*/, size_t /*count*/) {
  return NULL;
}
```

  </div>
</div>

**Parameters**



<table class="responsive">
  <tr><th colspan="2">Parameters</th></tr>
  <tr>
    <td><code>void*</code><br>        <code>destination</code></td>
    <td>The destination of the copied memory.</td>
  </tr>
  <tr>
    <td><code>int</code><br>        <code>byte_value</code></td>
    <td> </td>  </tr>
  <tr>
    <td><code>size_t</code><br>        <code>count</code></td>
    <td>The number of sequential bytes to be set.</td>
  </tr>
</table>

### SbMemoryUnmap

**Description**

Unmap `size_bytes` of physical pages starting from `virtual_address`,
returning `true` on success. After this function completes,
[virtual_address, virtual_address + size_bytes) will not be read/writable.
This function can unmap multiple contiguous regions that were mapped with
separate calls to <code><a href="#sbmemorymap">SbMemoryMap()</a></code>. For example, if one call to
`SbMemoryMap(0x1000)` returns `(void*)0xA000`, and another call to
`SbMemoryMap(0x1000)` returns `(void*)0xB000`,
`SbMemoryUnmap(0xA000, 0x2000)` should free both regions.

**Declaration and definitions**

<div class="mdl-tabs mdl-js-tabs mdl-js-ripple-effect">
  <div class="mdl-tabs__tab-bar">
    <a href="#SbMemoryUnmap-declaration" class="mdl-tabs__tab is-active">Declaration</a>
    <a href="#SbMemoryUnmap-stub" class="mdl-tabs__tab">stub</a>
  </div>
  <div class="mdl-tabs__panel is-active" id="SbMemoryUnmap-declaration">
<pre>
SB_EXPORT bool SbMemoryUnmap(void* virtual_address, int64_t size_bytes);
</pre>
</div>
  <div class="mdl-tabs__panel" id="SbMemoryUnmap-stub">

```
#include "starboard/memory.h"

bool SbMemoryUnmap(void* /*virtual_address*/, int64_t /*size_bytes*/) {
  return false;
}
```

  </div>
</div>

**Parameters**



<table class="responsive">
  <tr><th colspan="2">Parameters</th></tr>
  <tr>
    <td><code>void*</code><br>
        <code>virtual_address</code></td>
    <td> </td>
  </tr>
  <tr>
    <td><code>int64_t</code><br>
        <code>size_bytes</code></td>
    <td> </td>
  </tr>
</table>

