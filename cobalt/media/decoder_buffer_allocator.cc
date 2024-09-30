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

#include "base/logging.h"
#include "cobalt/math/size.h"
#include "media/base/starboard_utils.h"
#include "starboard/common/allocator.h"
#include "starboard/configuration.h"
#include "starboard/media.h"
#include "starboard/memory.h"

namespace cobalt {
namespace media {

namespace {

const bool kEnableAllocationLog = false;

// Used to determine if the memory allocated is large. The underlying logic can
// be different.
const size_t kSmallAllocationThreshold = 512;

}  // namespace

bool CreateSbMediaIsBufferPoolAllocateOnDemandWithHistogram(
    MediaMetricsProvider& media_metrics_provider) {
  media_metrics_provider.StartTrackingAction(
      MediaAction::SBMEDIA_BUFFER_POOL_ALLOCATE_ON_DEMAND);
  auto is_memory_pool_allocated_on_demand =
      SbMediaIsBufferPoolAllocateOnDemand();
  media_metrics_provider.EndTrackingAction(
      MediaAction::SBMEDIA_BUFFER_POOL_ALLOCATE_ON_DEMAND);
  return is_memory_pool_allocated_on_demand;
}

int CreateSbMediaGetInitialBufferCapacityWithHistogram(
    MediaMetricsProvider& media_metrics_provider) {
  media_metrics_provider.StartTrackingAction(
      MediaAction::SBMEDIA_GET_INIT_BUFFER_CAPACITY);
  auto initial_capacity = SbMediaGetInitialBufferCapacity();
  media_metrics_provider.EndTrackingAction(
      MediaAction::SBMEDIA_GET_INIT_BUFFER_CAPACITY);
  return initial_capacity;
}

bool CreateSbMediaGetBufferAllocationUnitWithHistogram(
    MediaMetricsProvider& media_metrics_provider) {
  media_metrics_provider.StartTrackingAction(
      MediaAction::SBMEDIA_GET_BUFFER_ALLOCATION_UNIT);
  auto allocation_unit = SbMediaGetBufferAllocationUnit();
  media_metrics_provider.EndTrackingAction(
      MediaAction::SBMEDIA_GET_BUFFER_ALLOCATION_UNIT);
  return allocation_unit;
}

DecoderBufferAllocator::DecoderBufferAllocator()
    : is_memory_pool_allocated_on_demand_(
          CreateSbMediaIsBufferPoolAllocateOnDemandWithHistogram(
              media_metrics_provider_)),
      initial_capacity_(CreateSbMediaGetInitialBufferCapacityWithHistogram(
          media_metrics_provider_)),
      allocation_unit_(CreateSbMediaGetBufferAllocationUnitWithHistogram(
          media_metrics_provider_)) {
  if (is_memory_pool_allocated_on_demand_) {
    DLOG(INFO) << "Allocated media buffer pool on demand.";
    Allocator::Set(this);
    return;
  }

  base::AutoLock scoped_lock(mutex_);
  EnsureReuseAllocatorIsCreated();
  Allocator::Set(this);
}

DecoderBufferAllocator::~DecoderBufferAllocator() {
  Allocator::Set(nullptr);

  base::AutoLock scoped_lock(mutex_);

  if (reuse_allocator_) {
    DCHECK_EQ(reuse_allocator_->GetAllocated(), 0);
    reuse_allocator_.reset();
  }
}

void DecoderBufferAllocator::Suspend() {
  if (is_memory_pool_allocated_on_demand_) {
    return;
  }

  base::AutoLock scoped_lock(mutex_);

  if (reuse_allocator_ && reuse_allocator_->GetAllocated() == 0) {
    DLOG(INFO) << "Freed " << reuse_allocator_->GetCapacity()
               << " bytes of media buffer pool `on suspend`.";
    reuse_allocator_.reset();
  }
}

void DecoderBufferAllocator::Resume() {
  if (is_memory_pool_allocated_on_demand_) {
    return;
  }

  base::AutoLock scoped_lock(mutex_);
  EnsureReuseAllocatorIsCreated();
}

void* DecoderBufferAllocator::Allocate(size_t size, size_t alignment) {
  base::AutoLock scoped_lock(mutex_);

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

  base::AutoLock scoped_lock(mutex_);

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
  media_metrics_provider_.StartTrackingAction(
      MediaAction::SBMEDIA_GET_AUDIO_BUFFER_BUDGET);
  int audio_buffer_budget = SbMediaGetAudioBufferBudget();
  media_metrics_provider_.EndTrackingAction(
      MediaAction::SBMEDIA_GET_AUDIO_BUFFER_BUDGET);
  return audio_buffer_budget;
}

int DecoderBufferAllocator::GetBufferAlignment() const {
#if SB_API_VERSION < 16
  return SbMediaGetBufferAlignment();
#else
  return sizeof(void*);
#endif  // SB_API_VERSION < 16
}

int DecoderBufferAllocator::GetBufferPadding() const {
  return SbMediaGetBufferPadding();
}

base::TimeDelta
DecoderBufferAllocator::GetBufferGarbageCollectionDurationThreshold() const {
  media_metrics_provider_.StartTrackingAction(
      MediaAction::SBMEDIA_GET_BUFFER_GARBAGE_COLLECTION_DURATION_THRESHOLD);
  int64_t buffer_garbage_collection_duration_threshold =
      SbMediaGetBufferGarbageCollectionDurationThreshold();
  media_metrics_provider_.EndTrackingAction(
      MediaAction::SBMEDIA_GET_BUFFER_GARBAGE_COLLECTION_DURATION_THRESHOLD);
  return base::TimeDelta::FromMicroseconds(
      buffer_garbage_collection_duration_threshold);
}

int DecoderBufferAllocator::GetProgressiveBufferBudget(
    SbMediaVideoCodec codec, int resolution_width, int resolution_height,
    int bits_per_pixel) const {
  media_metrics_provider_.StartTrackingAction(
      MediaAction::SBMEDIA_GET_PROGRESSIVE_BUFFER_BUDGET);
  int progressive_buffer_budget = SbMediaGetProgressiveBufferBudget(
      codec, resolution_width, resolution_height, bits_per_pixel);
  media_metrics_provider_.EndTrackingAction(
      MediaAction::SBMEDIA_GET_PROGRESSIVE_BUFFER_BUDGET);
  return progressive_buffer_budget;
}

int DecoderBufferAllocator::GetVideoBufferBudget(SbMediaVideoCodec codec,
                                                 int resolution_width,
                                                 int resolution_height,
                                                 int bits_per_pixel) const {
  media_metrics_provider_.StartTrackingAction(
      MediaAction::SBMEDIA_GET_VIDEO_BUFFER_BUDGET);
  int video_buffer_budget = SbMediaGetVideoBufferBudget(
      codec, resolution_width, resolution_height, bits_per_pixel);
  media_metrics_provider_.EndTrackingAction(
      MediaAction::SBMEDIA_GET_VIDEO_BUFFER_BUDGET);
  return video_buffer_budget;
}

size_t DecoderBufferAllocator::GetAllocatedMemory() const {
  base::AutoLock scoped_lock(mutex_);
  return reuse_allocator_ ? reuse_allocator_->GetAllocated() : 0;
}

size_t DecoderBufferAllocator::GetCurrentMemoryCapacity() const {
  base::AutoLock scoped_lock(mutex_);
  return reuse_allocator_ ? reuse_allocator_->GetCapacity() : 0;
}

size_t DecoderBufferAllocator::GetMaximumMemoryCapacity() const {
  base::AutoLock scoped_lock(mutex_);

  if (reuse_allocator_) {
    return std::max<size_t>(reuse_allocator_->max_capacity(),
                            max_buffer_capacity_);
  }
  return max_buffer_capacity_;
}

void DecoderBufferAllocator::EnsureReuseAllocatorIsCreated() {
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
