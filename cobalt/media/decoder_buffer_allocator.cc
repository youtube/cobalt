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

#include "cobalt/media/decoder_buffer_allocator.h"

#include <algorithm>
#include <vector>

#include "cobalt/math/size.h"
#include "media/base/starboard_utils.h"
#include "starboard/common/allocator.h"
#include "starboard/configuration.h"
#include "starboard/media.h"
#include "starboard/memory.h"

namespace cobalt {
namespace media {

namespace {

using starboard::ScopedLock;

const bool kEnableAllocationLog = false;

const size_t kAllocationRecordGranularity = 512 * 1024;
// Used to determine if the memory allocated is large. The underlying logic can
// be different.
const size_t kSmallAllocationThreshold = 512;

}  // namespace

DecoderBufferAllocator::DecoderBufferAllocator()
    : using_memory_pool_(SbMediaIsBufferUsingMemoryPool()),
      is_memory_pool_allocated_on_demand_(
          SbMediaIsBufferPoolAllocateOnDemand()),
      initial_capacity_(SbMediaGetInitialBufferCapacity()),
      allocation_unit_(SbMediaGetBufferAllocationUnit()) {
  if (!using_memory_pool_) {
    DLOG(INFO) << "Allocated media buffer memory using malloc* functions.";
    Allocator::Set(this);
    return;
  }

  if (is_memory_pool_allocated_on_demand_) {
    DLOG(INFO) << "Allocated media buffer pool on demand.";
    Allocator::Set(this);
    return;
  }

  ScopedLock scoped_lock(mutex_);
  EnsureReuseAllocatorIsCreated();
  Allocator::Set(this);
}

DecoderBufferAllocator::~DecoderBufferAllocator() {
  Allocator::Set(nullptr);

  if (!using_memory_pool_) {
    return;
  }

  ScopedLock scoped_lock(mutex_);

  if (reuse_allocator_) {
    DCHECK_EQ(reuse_allocator_->GetAllocated(), 0);
    reuse_allocator_.reset();
  }
}

void DecoderBufferAllocator::Suspend() {
  if (!using_memory_pool_ || is_memory_pool_allocated_on_demand_) {
    return;
  }

  ScopedLock scoped_lock(mutex_);

  if (reuse_allocator_ && reuse_allocator_->GetAllocated() == 0) {
    DLOG(INFO) << "Freed " << reuse_allocator_->GetCapacity()
               << " bytes of media buffer pool `on suspend`.";
    reuse_allocator_.reset();
  }
}

void DecoderBufferAllocator::Resume() {
  if (!using_memory_pool_ || is_memory_pool_allocated_on_demand_) {
    return;
  }

  ScopedLock scoped_lock(mutex_);
  EnsureReuseAllocatorIsCreated();
}

void* DecoderBufferAllocator::Allocate(size_t size, size_t alignment) {
  if (!using_memory_pool_) {
    sbmemory_bytes_used_.fetch_add(size);
    void* p = nullptr;
    posix_memalign(&p, alignment, size);
    CHECK(p);
    return p;
  }

  ScopedLock scoped_lock(mutex_);

  EnsureReuseAllocatorIsCreated();

  void* p = reuse_allocator_->Allocate(size, alignment);
  CHECK(p);

  LOG_IF(INFO, kEnableAllocationLog)
      << "Media Allocation Log " << p << " " << size << " " << alignment << " ";
  return p;
}

void DecoderBufferAllocator::Free(void* p, size_t size) {
  if (p == nullptr) {
    DCHECK_EQ(size, 0);
    return;
  }

  if (!using_memory_pool_) {
    sbmemory_bytes_used_.fetch_sub(size);
    free(p);
    return;
  }

  ScopedLock scoped_lock(mutex_);

  DCHECK(reuse_allocator_);

  LOG_IF(INFO, kEnableAllocationLog) << "Media Allocation Log " << p;

  reuse_allocator_->Free(p);
  if (is_memory_pool_allocated_on_demand_) {
    if (reuse_allocator_->GetAllocated() == 0) {
      DLOG(INFO) << "Freed " << reuse_allocator_->GetCapacity()
                 << " bytes of media buffer pool `on demand`.";
      reuse_allocator_.reset();
    }
  }
}

int DecoderBufferAllocator::GetAudioBufferBudget() const {
  return SbMediaGetAudioBufferBudget();
}

int DecoderBufferAllocator::GetBufferAlignment() const {
#if SB_API_VERSION < 16
#if SB_API_VERSION >= 14
  return SbMediaGetBufferAlignment();
#else   // SB_API_VERSION >= 14
  return std::max(SbMediaGetBufferAlignment(kSbMediaTypeAudio),
                  SbMediaGetBufferAlignment(kSbMediaTypeVideo));
#endif  // SB_API_VERSION >= 14
#else
  return sizeof(void*);
#endif  // SB_API_VERSION < 16
}

int DecoderBufferAllocator::GetBufferPadding() const {
#if SB_API_VERSION >= 14
  return SbMediaGetBufferPadding();
#else   // SB_API_VERSION >= 14
  return std::max(SbMediaGetBufferPadding(kSbMediaTypeAudio),
                  SbMediaGetBufferPadding(kSbMediaTypeVideo));
#endif  // SB_API_VERSION >= 14
}

base::TimeDelta
DecoderBufferAllocator::GetBufferGarbageCollectionDurationThreshold() const {
  return base::TimeDelta::FromMicroseconds(
      SbMediaGetBufferGarbageCollectionDurationThreshold());
}

int DecoderBufferAllocator::GetProgressiveBufferBudget(
    SbMediaVideoCodec codec, int resolution_width, int resolution_height,
    int bits_per_pixel) const {
  return SbMediaGetProgressiveBufferBudget(codec, resolution_width,
                                           resolution_height, bits_per_pixel);
}

int DecoderBufferAllocator::GetVideoBufferBudget(SbMediaVideoCodec codec,
                                                 int resolution_width,
                                                 int resolution_height,
                                                 int bits_per_pixel) const {
  return SbMediaGetVideoBufferBudget(codec, resolution_width, resolution_height,
                                     bits_per_pixel);
}

size_t DecoderBufferAllocator::GetAllocatedMemory() const {
  if (!using_memory_pool_) {
    return sbmemory_bytes_used_.load();
  }
  ScopedLock scoped_lock(mutex_);
  return reuse_allocator_ ? reuse_allocator_->GetAllocated() : 0;
}

size_t DecoderBufferAllocator::GetCurrentMemoryCapacity() const {
  if (!using_memory_pool_) {
    return sbmemory_bytes_used_.load();
  }
  ScopedLock scoped_lock(mutex_);
  return reuse_allocator_ ? reuse_allocator_->GetCapacity() : 0;
}

size_t DecoderBufferAllocator::GetMaximumMemoryCapacity() const {
  ScopedLock scoped_lock(mutex_);

  if (reuse_allocator_) {
    return std::max<size_t>(reuse_allocator_->max_capacity(),
                            max_buffer_capacity_);
  }
  return max_buffer_capacity_;
}

void DecoderBufferAllocator::EnsureReuseAllocatorIsCreated() {
  mutex_.DCheckAcquired();

  if (reuse_allocator_) {
    return;
  }

  reuse_allocator_.reset(new BidirectionalFitReuseAllocator(
      &fallback_allocator_, initial_capacity_, kSmallAllocationThreshold,
      allocation_unit_, 0));
  DLOG(INFO) << "Allocated " << initial_capacity_
             << " bytes for media buffer pool.";
}

}  // namespace media
}  // namespace cobalt
