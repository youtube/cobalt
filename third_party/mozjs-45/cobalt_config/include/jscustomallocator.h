// Copyright 2017 Google Inc. All Rights Reserved.
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

#ifndef gc_JSCUSTOMALLOCATOR_H
#define gc_JSCUSTOMALLOCATOR_H

#include <algorithm>

#include "memory_allocator_reporter.h"
#include "starboard/memory.h"
#include "starboard/string.h"

namespace js {
namespace oom {

// Code imported from default allocator.  These identifiers must be supplied
// to SpiderMonkey common code.

/*
 * To make testing OOM in certain helper threads more effective,
 * allow restricting the OOM testing to a certain helper thread
 * type. This allows us to fail e.g. in off-thread script parsing
 * without causing an OOM in the main thread first.
 */
enum ThreadType {
    THREAD_TYPE_NONE = 0,       // 0
    THREAD_TYPE_MAIN,           // 1
    THREAD_TYPE_ASMJS,          // 2
    THREAD_TYPE_ION,            // 3
    THREAD_TYPE_PARSE,          // 4
    THREAD_TYPE_COMPRESS,       // 5
    THREAD_TYPE_GCHELPER,       // 6
    THREAD_TYPE_GCPARALLEL,     // 7
    THREAD_TYPE_MAX             // Used to check shell function arguments
};

/*
 * Getter/Setter functions to encapsulate mozilla::ThreadLocal,
 * implementation is in jsutil.cpp.
 */
# if defined(DEBUG) || defined(JS_OOM_BREAKPOINT)
extern bool InitThreadType(void);
extern void SetThreadType(ThreadType);
extern uint32_t GetThreadType(void);
# else
inline bool InitThreadType(void) { return true; }
inline void SetThreadType(ThreadType t) {};
inline uint32_t GetThreadType(void) { return 0; }
# endif

}  // namespace oom
}  // namespace js

# if defined(DEBUG) || defined(JS_OOM_BREAKPOINT)

/*
 * In order to test OOM conditions, when the testing function
 * oomAfterAllocations COUNT is passed, we fail continuously after the NUM'th
 * allocation from now.
 */
extern JS_PUBLIC_DATA(uint32_t) OOM_maxAllocations; /* set in builtin/TestingFunctions.cpp */
extern JS_PUBLIC_DATA(uint32_t) OOM_counter; /* data race, who cares. */
extern JS_PUBLIC_DATA(bool) OOM_failAlways;

#define JS_OOM_CALL_BP_FUNC() do {} while(0)

namespace js {
namespace oom {

extern JS_PUBLIC_DATA(uint32_t) targetThread;

static inline bool
IsThreadSimulatingOOM()
{
    return false;
}

static inline bool
IsSimulatedOOMAllocation()
{
    return false;
}

static inline bool
ShouldFailWithOOM()
{
    return false;
}

}  // namespace oom
}  // namespace js

#  define JS_OOM_POSSIBLY_FAIL()                                              \
    do {                                                                      \
        if (js::oom::ShouldFailWithOOM())                                     \
            return nullptr;                                                   \
    } while (0)

#  define JS_OOM_POSSIBLY_FAIL_BOOL()                                         \
    do {                                                                      \
        if (js::oom::ShouldFailWithOOM())                                     \
            return false;                                                     \
    } while (0)

# else  // defined(DEBUG) || defined(JS_OOM_BREAKPOINT)

#  define JS_OOM_POSSIBLY_FAIL() do {} while(0)
#  define JS_OOM_POSSIBLY_FAIL_BOOL() do {} while(0)
namespace js {
namespace oom {
static inline bool IsSimulatedOOMAllocation() { return false; }
static inline bool ShouldFailWithOOM() { return false; }
}  // namespace oom
}  // namespace js

# endif  // defined(DEBUG) || defined(JS_OOM_BREAKPOINT)

namespace js {

struct MOZ_RAII AutoEnterOOMUnsafeRegion
{
    MOZ_NORETURN MOZ_COLD void crash(const char* reason);
};

}  // namespace js

static inline void* js_malloc(size_t bytes)
{
    size_t reservation_bytes = AllocationMetadata::GetReservationBytes(bytes);
    MemoryAllocatorReporter::Get()->UpdateTotalHeapSize(reservation_bytes);
    void* metadata = SbMemoryAllocate(reservation_bytes);
    AllocationMetadata::SetSizeToBaseAddress(metadata, reservation_bytes);
    return AllocationMetadata::GetUserAddressFromBaseAddress(metadata);
}

static inline void* js_calloc(size_t nmemb, size_t size)
{
    size_t total_size = nmemb * size;
    void* memory = js_malloc(total_size);
    if (memory) {
        SbMemorySet(memory, 0, total_size);
    }
    return memory;
}

static inline void* js_calloc(size_t bytes)
{
    return js_calloc(bytes, 1);
}

static inline void* js_realloc(void* p, size_t bytes)
{
  AllocationMetadata* metadata =
      AllocationMetadata::GetMetadataFromUserAddress(p);
  size_t current_size =
      AllocationMetadata::GetSizeOfAllocationFromMetadata(metadata);
  size_t adjusted_size = AllocationMetadata::GetReservationBytes(bytes);

  MemoryAllocatorReporter::Get()->UpdateTotalHeapSize(
      static_cast<ssize_t>(adjusted_size - current_size));
  void* new_ptr = SbMemoryReallocate(metadata, adjusted_size);
  AllocationMetadata::SetSizeToBaseAddress(new_ptr, adjusted_size);
  return AllocationMetadata::GetUserAddressFromBaseAddress(new_ptr);
}

static inline void js_free(void* p)
{
  if (p == NULL) {
    return;
  }
  AllocationMetadata* metadata =
      AllocationMetadata::GetMetadataFromUserAddress(p);
  MemoryAllocatorReporter::Get()->UpdateTotalHeapSize(-static_cast<ssize_t>(
      AllocationMetadata::GetSizeOfAllocationFromMetadata(metadata)));
  SbMemoryDeallocate(metadata);
}

static inline char* js_strdup(const char* s)
{
  size_t length = SbStringGetLength(s) + 1;
  char* new_ptr = reinterpret_cast<char*>(js_malloc(length));
  SbStringCopy(new_ptr, s, length);
  return new_ptr;
}

#endif  // gc_JSCUSTOMALLOCATOR_H
