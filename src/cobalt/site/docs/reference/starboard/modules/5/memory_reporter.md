---
layout: doc
title: "Starboard Module Reference: memory_reporter.h"
---

Provides an interface for memory reporting.

## Typedefs ##

### SbMemoryReporterOnAlloc ###

A function to report a memory allocation from SbMemoryAllocate(). Note that
operator new calls SbMemoryAllocate which will delegate to this callback.

#### Definition ####

```
typedef void(* SbMemoryReporterOnAlloc)(void *context, const void *memory, size_t size)
```

### SbMemoryReporterOnDealloc ###

A function to report a memory deallocation from SbMemoryDeallcoate(). Note that
operator delete calls SbMemoryDeallocate which will delegate to this callback.

#### Definition ####

```
typedef void(* SbMemoryReporterOnDealloc)(void *context, const void *memory)
```

### SbMemoryReporterOnMapMemory ###

A function to report a memory mapping from SbMemoryMap().

#### Definition ####

```
typedef void(* SbMemoryReporterOnMapMemory)(void *context, const void *memory, size_t size)
```

### SbMemoryReporterOnUnMapMemory ###

A function to report a memory unmapping from SbMemoryUnmap().

#### Definition ####

```
typedef void(* SbMemoryReporterOnUnMapMemory)(void *context, const void *memory, size_t size)
```

## Structs ##

### SbMemoryReporter ###

SbMemoryReporter allows memory reporting via user-supplied functions. The void*
context is passed to every call back. It's strongly recommended that C-Style
struct initialization is used so that the arguments can be typed check by the
compiler. For example, SbMemoryReporter mem_reporter = { MallocCallback, ....
context };

#### Members ####

*   `SbMemoryReporterOnAlloc on_alloc_cb`

    Callback to report allocations.
*   `SbMemoryReporterOnDealloc on_dealloc_cb`

    Callback to report deallocations.
*   `SbMemoryReporterOnMapMemory on_mapmem_cb`

    Callback to report memory map.
*   `SbMemoryReporterOnUnMapMemory on_unmapmem_cb`

    Callback to report memory unmap.
*   `void * context`

    Optional, is passed to callbacks as first argument.

## Functions ##

### SbMemorySetReporter ###

Sets the MemoryReporter. Any previous memory reporter is unset. No lifetime
management is done internally on input pointer.

Returns true if the memory reporter was set with no errors. If an error was
reported then check the log for why it failed.

Note that other than a thread-barrier-write of the input pointer, there is no
thread safety guarantees with this function due to performance considerations.
It's recommended that this be called once during the lifetime of the program, or
not at all. Do not delete the supplied pointer, ever. Example (Good):
SbMemoryReporter* mem_reporter = new ...; SbMemorySetReporter(&mem_reporter);
... SbMemorySetReporter(NULL); // allow value to leak. Example (Bad):
SbMemoryReporter* mem_reporter = new ...; SbMemorySetReporter(&mem_reporter);
... SbMemorySetReporter(NULL); delete mem_reporter; // May crash.

#### Declaration ####

```
bool SbMemorySetReporter(struct SbMemoryReporter *tracker)
```

