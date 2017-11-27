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

#ifndef COBALT_MEDIA_DECODER_BUFFER_ALLOCATOR_H_
#define COBALT_MEDIA_DECODER_BUFFER_ALLOCATOR_H_

#include "base/compiler_specific.h"
#include "cobalt/media/base/decoder_buffer.h"
#include "nb/bidirectional_fit_reuse_allocator.h"
#include "nb/starboard_memory_allocator.h"
#include "starboard/common/scoped_ptr.h"
#include "starboard/mutex.h"

namespace cobalt {
namespace media {

#if COBALT_MEDIA_BUFFER_INITIAL_CAPACITY > 0 || \
    COBALT_MEDIA_BUFFER_ALLOCATION_UNIT > 0
#define COBALT_MEDIA_BUFFER_USING_MEMORY_POOL 1
#endif  // COBALT_MEDIA_BUFFER_INITIAL_CAPACITY == 0 &&
        // COBALT_MEDIA_BUFFER_ALLOCATION_UNIT == 0

class DecoderBufferAllocator : public DecoderBuffer::Allocator {
 public:
  DecoderBufferAllocator();
  ~DecoderBufferAllocator() override;

  Allocations Allocate(size_t size, size_t alignment,
                       intptr_t context) override;
  void Free(Allocations allocations) override;

 private:
#if COBALT_MEDIA_BUFFER_USING_MEMORY_POOL
  class ReuseAllcator : public nb::BidirectionalFitReuseAllocator {
   public:
    ReuseAllcator(Allocator* fallback_allocator, std::size_t initial_capacity,
                  std::size_t allocation_increment);

    FreeBlockSet::iterator FindBestFreeBlock(
        std::size_t size, std::size_t alignment, intptr_t context,
        FreeBlockSet::iterator begin, FreeBlockSet::iterator end,
        bool* allocate_from_front) override;
  };

  void UpdateAllocationRecord(std::size_t blocks = 1) const;

  starboard::Mutex mutex_;
  nb::StarboardMemoryAllocator fallback_allocator_;
  starboard::scoped_ptr<ReuseAllcator> reuse_allcator_;
#endif  // COBALT_MEDIA_BUFFER_USING_MEMORY_POOL
};

}  // namespace media
}  // namespace cobalt

#endif  // COBALT_MEDIA_DECODER_BUFFER_ALLOCATOR_H_
