// Copyright 2016 The Cobalt Authors. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

// Module Overview: Memory Reporting API.
//
// Provides an interface for memory reporting.

#ifndef STARBOARD_MEMORY_REPORTER_H_
#define STARBOARD_MEMORY_REPORTER_H_

#include "starboard/configuration.h"
#include "starboard/export.h"
#include "starboard/types.h"

#ifdef __cplusplus
extern "C" {
#endif

///////////////////////////////////////////////////////////////////////////////
// These are callbacks used by SbMemoryReporter to report memory
// allocation and deallocation.
///////////////////////////////////////////////////////////////////////////////

// A function to report a memory allocation from SbMemoryAllocate(). Note that
// operator new calls SbMemoryAllocate which will delegate to this callback.
typedef void (*SbMemoryReporterOnAlloc)(void* context, const void* memory,
                                        size_t size);

// A function to report a memory deallocation from SbMemoryDeallcoate(). Note
// that operator delete calls SbMemoryDeallocate which will delegate to this
// callback.
typedef void (*SbMemoryReporterOnDealloc)(void* context, const void* memory);

// A function to report a memory mapping from SbMemoryMap().
typedef void (*SbMemoryReporterOnMapMemory)(void* context, const void* memory,
                                            size_t size);

// A function to report a memory unmapping from SbMemoryUnmap().
typedef void (*SbMemoryReporterOnUnMapMemory)(void* context,
                                              const void* memory,
                                              size_t size);

// SbMemoryReporter allows memory reporting via user-supplied functions.
// The void* context is passed to every call back.
// It's strongly recommended that C-Style struct initialization is used
// so that the arguments can be typed check by the compiler.
// For example,
//  SbMemoryReporter mem_reporter = { MallocCallback, .... context };
struct SbMemoryReporter {
  // Callback to report allocations.
  SbMemoryReporterOnAlloc on_alloc_cb;

  // Callback to report deallocations.
  SbMemoryReporterOnDealloc on_dealloc_cb;

  // Callback to report memory map.
  SbMemoryReporterOnMapMemory on_mapmem_cb;

  // Callback to report memory unmap.
  SbMemoryReporterOnUnMapMemory on_unmapmem_cb;

  // Optional, is passed to callbacks as first argument.
  void* context;
};

// Sets the MemoryReporter. Any previous memory reporter is unset. No lifetime
// management is done internally on input pointer.
//
// Returns true if the memory reporter was set with no errors. If an error
// was reported then check the log for why it failed.
//
// Note that other than a thread-barrier-write of the input pointer, there is
// no thread safety guarantees with this function due to performance
// considerations. It's recommended that this be called once during the
// lifetime of the program, or not at all. Do not delete the supplied pointer,
// ever.
// Example (Good):
//    SbMemoryReporter* mem_reporter = new ...;
//    SbMemorySetReporter(&mem_reporter);
//    ...
//    SbMemorySetReporter(NULL);  // allow value to leak.
// Example (Bad):
//    SbMemoryReporter* mem_reporter = new ...;
//    SbMemorySetReporter(&mem_reporter);
//    ...
//    SbMemorySetReporter(NULL);
//    delete mem_reporter;        // May crash.
SB_EXPORT bool SbMemorySetReporter(struct SbMemoryReporter* tracker);

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // STARBOARD_MEMORY_REPORTER_H_
