// Copyright 2017 The Cobalt Authors. All Rights Reserved.
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

#ifndef MEDIA_STARBOARD_DECODER_BUFFER_ALLOCATOR_H_
#define MEDIA_STARBOARD_DECODER_BUFFER_ALLOCATOR_H_

#include <memory>
#include <sstream>

#include "base/compiler_specific.h"
#include "base/synchronization/lock.h"
#include "base/thread_annotations.h"
#include "base/time/time.h"
#include "media/base/decoder_buffer.h"
#include "media/starboard/decoder_buffer_memory_info.h"
#include "starboard/media.h"

namespace media {

class DecoderBufferAllocator : public DecoderBuffer::Allocator,
                               public DecoderBufferMemoryInfo {
 public:
  enum class Type {
    kGlobal,  // The global allocator calls `Allocator::Set(this)` to register
              // itself in the ctor
    kLocal,
  };

  // Manages the details of the Allocate() and Free(), to allow
  // DecoderBufferAllocator to adopt different strategies at runtime.
  // The class isn't required to be thread safe, and relies on
  // DecoderBufferAllocator to properly guard calls to its member functions with
  // mutex.
  class Strategy {
   public:
    virtual ~Strategy() {}
    virtual void* Allocate(DemuxerStream::Type type,
                           size_t size,
                           size_t alignment) = 0;
    virtual void Free(DemuxerStream::Type type, void* p) = 0;

    virtual size_t GetCapacity() const = 0;
    virtual size_t GetAllocated() const = 0;
  };

  explicit DecoderBufferAllocator(Type type = Type::kGlobal);
  DecoderBufferAllocator(Type type,
                         bool is_memory_pool_allocated_on_demand,
                         int initial_capacity,
                         int allocation_unit);
  ~DecoderBufferAllocator() override;

  void Suspend();
  void Resume();

  // DecoderBuffer::Allocator methods.
  void* Allocate(DemuxerStream::Type type,
                 size_t size,
                 size_t alignment) override;
  void Free(void* p, size_t size) override;

  int GetAudioBufferBudget() const override;
  int GetBufferAlignment() const override;
  int GetBufferPadding() const override;
  base::TimeDelta GetBufferGarbageCollectionDurationThreshold() const override;
  int GetProgressiveBufferBudget(VideoCodec codec,
                                 int resolution_width,
                                 int resolution_height,
                                 int bits_per_pixel) const override;
  int GetVideoBufferBudget(VideoCodec codec,
                           int resolution_width,
                           int resolution_height,
                           int bits_per_pixel) const override;

  // DecoderBufferMemoryInfo methods.
  size_t GetAllocatedMemory() const override LOCKS_EXCLUDED(mutex_);
  size_t GetCurrentMemoryCapacity() const override LOCKS_EXCLUDED(mutex_);
  size_t GetMaximumMemoryCapacity() const override LOCKS_EXCLUDED(mutex_);

 private:
  void EnsureStrategyIsCreated() EXCLUSIVE_LOCKS_REQUIRED(mutex_);

#if !defined(COBALT_BUILD_TYPE_GOLD)
  void TryFlushAllocationLog_Locked() EXCLUSIVE_LOCKS_REQUIRED(mutex_);
#endif  // !defined(COBALT_BUILD_TYPE_GOLD)

  const Type type_;
  const bool is_memory_pool_allocated_on_demand_;
  const int initial_capacity_;
  const int allocation_unit_;

  mutable base::Lock mutex_;
  std::unique_ptr<Strategy> strategy_ GUARDED_BY(mutex_);

#if !defined(COBALT_BUILD_TYPE_GOLD)
  // The following variables are used for comprehensive logging of allocation
  // operations.
  std::stringstream pending_allocation_operations_ GUARDED_BY(mutex_);
  int pending_allocation_operations_count_ GUARDED_BY(mutex_) = 0;
  int allocation_operation_index_ GUARDED_BY(mutex_) = 0;
#endif  // !defined(COBALT_BUILD_TYPE_GOLD)
};

}  // namespace media

#endif  // MEDIA_STARBOARD_DECODER_BUFFER_ALLOCATOR_H_
