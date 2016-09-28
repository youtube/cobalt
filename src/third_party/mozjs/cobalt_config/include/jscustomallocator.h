// Copyright 2016 Google Inc. All Rights Reserved.
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

#include "jstypes.h"
#include "memory_allocator_reporter.h"
#include "starboard/memory.h"
#include "starboard/string.h"

#define JS_OOM_POSSIBLY_FAIL() do {} while(0)
#define JS_OOM_POSSIBLY_FAIL_REPORT(cx) do {} while(0)

static JS_INLINE void* js_malloc(size_t bytes) {
  size_t reservation_bytes = AllocationMetadata::GetReservationBytes(bytes);
  MemoryAllocatorReporter::Get()->UpdateAllocatedBytes(reservation_bytes);
  void* metadata = SbMemoryAllocate(reservation_bytes);
  AllocationMetadata::SetSizeToBaseAddress(metadata, reservation_bytes);
  return AllocationMetadata::GetUserAddressFromBaseAddress(metadata);
}

static JS_INLINE void* js_calloc(size_t nmemb, size_t size) {
  size_t total_size = nmemb * size;
  void* memory = js_malloc(total_size);
  if (memory) {
    SbMemorySet(memory, 0, total_size);
  }
  return memory;
}

static JS_INLINE void* js_calloc(size_t bytes) {
  return js_calloc(bytes, 1);
}

static JS_INLINE void js_free(void* p) {
  if (p == NULL) {
    return;
  }

  AllocationMetadata* metadata =
      AllocationMetadata::GetMetadataFromUserAddress(p);
  MemoryAllocatorReporter::Get()->UpdateAllocatedBytes(-static_cast<ssize_t>(
      AllocationMetadata::GetSizeOfAllocationFromMetadata(metadata)));
  SbMemoryFree(metadata);
}

static JS_INLINE void* js_realloc(void* p, size_t bytes) {
  AllocationMetadata* metadata =
      AllocationMetadata::GetMetadataFromUserAddress(p);
  size_t current_size =
      AllocationMetadata::GetSizeOfAllocationFromMetadata(metadata);
  size_t adjusted_size = AllocationMetadata::GetReservationBytes(bytes);

  MemoryAllocatorReporter::Get()->UpdateAllocatedBytes(
      static_cast<ssize_t>(adjusted_size - current_size));
  void* new_ptr = SbMemoryReallocate(metadata, adjusted_size);
  AllocationMetadata::SetSizeToBaseAddress(new_ptr, adjusted_size);
  return AllocationMetadata::GetUserAddressFromBaseAddress(new_ptr);
}

static JS_INLINE char* js_strdup(char* s) {
  size_t length = SbStringGetLength(s) + 1;

  char* new_ptr = reinterpret_cast<char*>(js_malloc(length));
  SbStringCopy(new_ptr, s, length);
  return new_ptr;
}

#endif /* gc_JSCUSTOMALLOCATOR_H */
