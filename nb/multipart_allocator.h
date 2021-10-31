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

#ifndef NB_MULTIPART_ALLOCATOR_H_
#define NB_MULTIPART_ALLOCATOR_H_

#include "starboard/configuration.h"
#include "starboard/types.h"

namespace nb {

// The class can fulfill allocation request by more than one blocks.  It also
// provides a nested class to manage such allocations.
class MultipartAllocator {
 public:
  class Allocations {
   public:
    Allocations()
        : number_of_buffers_(0), buffers_(NULL), buffer_sizes_(NULL) {}
    Allocations(void* buffer, int buffer_size);
    Allocations(int number_of_buffers, void** buffers, const int* buffer_sizes);
    Allocations(const Allocations& that);
    ~Allocations();

    Allocations& operator=(const Allocations& that);

    int size() const;

    void* const* buffers() { return buffers_; }
    const void* const* buffers() const { return buffers_; }

    const int* buffer_sizes() const { return buffer_sizes_; }

    int number_of_buffers() const { return number_of_buffers_; }

    void ShrinkTo(int size);
    // Write |size| bytes from |src| to buffers start from |destination_offset|.
    void Write(int destination_offset, const void* src, int size);
    // Read data from the Allocations into |destination|.  |destination| is
    // guaranteed to have enough space to hold all data contained.
    void Read(void* destination) const;

   private:
    void Assign(int number_of_buffers, void** buffers, const int* buffer_sizes);
    void Destroy();

    int number_of_buffers_;
    void** buffers_;
    int* buffer_sizes_;
    // Store data in the class to avoid allocate memory when there is only one
    // buffer.  In this case |buffers_| and |buffer_sizes_| will point to
    // |buffer_| and |buffer_size_| respectively.
    void* buffer_;
    int buffer_size_;
  };

  virtual ~MultipartAllocator() {}

  // Allocate a memory block that contains at least |size| bytes and its
  // address is aligned to |alignment|.  It returns an empty Allocations object
  // on failure.
  virtual Allocations Allocate(size_t size,
                               size_t alignment,
                               intptr_t context) = 0;
  // Free a memory block previously allocated by calling Allocate().
  virtual void Free(Allocations allocations) = 0;
};

}  // namespace nb

#endif  // NB_MULTIPART_ALLOCATOR_H_
