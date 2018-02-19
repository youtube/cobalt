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

#include "starboard/mutex.h"
#include "starboard/types.h"

class AllocationMetadata {
 public:
  static AllocationMetadata* GetMetadataFromUserAddress(void* ptr);
  static void* GetUserAddressFromBaseAddress(void* base_ptr);
  static int64_t GetReservationBytes(int64_t bytes_requested) {
    return sizeof(AllocationMetadata) + bytes_requested;
  }
  static int64_t GetSizeOfAllocationFromMetadata(AllocationMetadata* metadata) {
    return metadata ? metadata->size_requested() : 0;
  }
  static void SetSizeToBaseAddress(void* base_ptr, int64_t size);

  int64_t size_requested() { return size_requested_; }
  void set_size_requested(int64_t size) { size_requested_ = size; }

 private:
  // Bytes requested by the underlying allocator.
  int64_t size_requested_;
};

// Reporter that is used to report memory allocation.
class MemoryAllocatorReporter {
 public:
  static MemoryAllocatorReporter* Get();

  MemoryAllocatorReporter() : total_heap_size_(0) {}

  void UpdateTotalHeapSize(int64_t bytes);
  int64_t GetTotalHeapSize();

 private:
  starboard::Mutex mutex_;
  // The total heap size in bytes.
  int64_t total_heap_size_;
};

#endif /* MemoryAllocatorReporter_h */
