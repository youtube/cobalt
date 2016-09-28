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

#ifndef MemoryAllocatorReporter_h
#define MemoryAllocatorReporter_h

#include <sys/types.h>

#include "starboard/mutex.h"

class AllocationMetadata {
 public:
  static AllocationMetadata* GetMetadataFromUserAddress(void* ptr);
  static void* GetUserAddressFromBaseAddress(void* base_ptr);
  static size_t GetReservationBytes(size_t bytes_requested) {
    return sizeof(AllocationMetadata) + bytes_requested;
  }
  static size_t GetSizeOfAllocationFromMetadata(AllocationMetadata* metadata) {
    return metadata ? metadata->size_requested() : 0;
  }
  static void SetSizeToBaseAddress(void* base_ptr, size_t size);

  size_t size_requested() { return size_requested_; }
  void set_size_requested(size_t size) { size_requested_ = size; }

 private:
  // Bytes requested by the underlying allocator.
  size_t size_requested_;
};

// Reporter that is used to report memory allocation.
class MemoryAllocatorReporter {
 public:
  static MemoryAllocatorReporter* Get();

  MemoryAllocatorReporter()
      : current_bytes_allocated_(0), current_bytes_mapped_(0) {}

  void UpdateAllocatedBytes(ssize_t bytes);
  ssize_t GetCurrentBytesAllocated();

  void UpdateMappedBytes(ssize_t bytes);
  ssize_t GetCurrentBytesMapped();

 private:
  starboard::Mutex mutex_;
  ssize_t current_bytes_allocated_;
  ssize_t current_bytes_mapped_;
};

#endif /* MemoryAllocatorReporter_h */
