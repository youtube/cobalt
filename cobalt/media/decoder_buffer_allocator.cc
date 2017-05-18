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

#include "cobalt/media/decoder_buffer_allocator.h"

#include "starboard/common/scoped_ptr.h"
#include "starboard/configuration.h"
#include "starboard/memory.h"

namespace cobalt {
namespace media {

namespace {
const bool kPreAllocateAllMemory = true;
}  // namespace

DecoderBufferAllocator::DecoderBufferAllocator() : memory_block_(NULL) {
  if (COBALT_MEDIA_BUFFER_INITIAL_CAPACITY > 0) {
    memory_block_ = SbMemoryAllocateAligned(
        DecoderBuffer::kAlignmentSize, COBALT_MEDIA_BUFFER_INITIAL_CAPACITY);
  }

  if (COBALT_MEDIA_BUFFER_INITIAL_CAPACITY > 0 ||
      COBALT_MEDIA_BUFFER_ALLOCATION_UNIT > 0) {
    // TODO: Support COBALT_MEDIA_BUFFER_ALLOCATION_UNIT > 0.
    memory_pool_.set(starboard::make_scoped_ptr(
        new nb::MemoryPool(memory_block_, COBALT_MEDIA_BUFFER_INITIAL_CAPACITY,
                           kPreAllocateAllMemory)));
  }
}

DecoderBufferAllocator::~DecoderBufferAllocator() {
  if (memory_pool_.is_valid()) {
    DCHECK_EQ(memory_pool_->GetAllocated(), 0);
  }

  if (memory_block_) {
    SbMemoryDeallocateAligned(memory_block_);
  }
}

void* DecoderBufferAllocator::Allocate(Type type, size_t size,
                                       size_t alignment) {
  UNREFERENCED_PARAMETER(type);
  if (memory_pool_.is_valid()) {
    return memory_pool_->Allocate(size, alignment);
  }

  return SbMemoryAllocateAligned(alignment, size);
}

void DecoderBufferAllocator::Free(Type type, void* ptr) {
  UNREFERENCED_PARAMETER(type);
  if (memory_pool_.is_valid()) {
    memory_pool_->Free(ptr);
    return;
  }

  SbMemoryDeallocateAligned(ptr);
}

}  // namespace media
}  // namespace cobalt
