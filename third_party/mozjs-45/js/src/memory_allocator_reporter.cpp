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

#include "memory_allocator_reporter.h"

#include "starboard/once.h"

namespace {
// Control to initialize s_instance.
SbOnceControl s_instance_control = SB_ONCE_INITIALIZER;
MemoryAllocatorReporter* s_instance = NULL;

void Initialize() {
  s_instance = new MemoryAllocatorReporter();
}

void* OffsetPointer(void* base, int64_t offset) {
  uintptr_t base_as_int = reinterpret_cast<uintptr_t>(base);
#if defined(STARBOARD)
    return reinterpret_cast<void*>(base_as_int + static_cast<uintptr_t>(offset));
#else
    return reinterpret_cast<void*>(base_as_int + offset);
#endif
}
}  // namespace

AllocationMetadata* AllocationMetadata::GetMetadataFromUserAddress(void* ptr) {
  if (ptr == NULL) {
    return NULL;
  }

  // The metadata lives just in front of the data.
  void* meta_addr =
      OffsetPointer(ptr, -static_cast<int64_t>(sizeof(AllocationMetadata)));
  return reinterpret_cast<AllocationMetadata*>(meta_addr);
}

void* AllocationMetadata::GetUserAddressFromBaseAddress(void* base_ptr) {
  void* adjusted_base =
      OffsetPointer(base_ptr, static_cast<int64_t>(sizeof(AllocationMetadata)));
  return adjusted_base;
}

void AllocationMetadata::SetSizeToBaseAddress(void* base_ptr, int64_t size) {
  if (base_ptr) {
    AllocationMetadata* metadata =
        reinterpret_cast<AllocationMetadata*>(base_ptr);
    metadata->set_size_requested(size);
  }
}

void MemoryAllocatorReporter::UpdateAllocatedBytes(int64_t bytes) {
  starboard::ScopedLock lock(mutex_);
  current_bytes_allocated_ += bytes;
}

int64_t MemoryAllocatorReporter::GetCurrentBytesAllocated() {
  starboard::ScopedLock lock(mutex_);
  return current_bytes_allocated_;
}

// static
MemoryAllocatorReporter* MemoryAllocatorReporter::Get() {
  SbOnce(&s_instance_control, &Initialize);
  return s_instance;
}
