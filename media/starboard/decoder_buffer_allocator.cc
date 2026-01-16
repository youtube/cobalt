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

#include "media/starboard/decoder_buffer_allocator.h"

#include <algorithm>

#include "base/feature_list.h"
#include "base/logging.h"
#include "build/build_config.h"
#include "media/base/media_switches.h"
#include "media/base/video_codecs.h"
#include "media/starboard/bidirectional_fit_decoder_buffer_allocator_strategy.h"
#include "media/starboard/media_buffer_pool_decoder_buffer_allocator_strategy.h"
#include "media/starboard/starboard_utils.h"
#include "starboard/common/allocator.h"
#include "starboard/common/experimental/media_buffer_pool.h"
#include "starboard/common/in_place_reuse_allocator_base.h"
#include "starboard/common/log.h"
#include "starboard/common/reuse_allocator_base.h"
#include "starboard/configuration.h"
#include "starboard/media.h"

namespace media {

namespace {

// Used to determine if the memory allocated is large. The underlying logic can
// be different.
const size_t kSmallAllocationThreshold = 512;

const char* ToString(bool value) {
  return value ? "enabled" : "disabled";
}

}  // namespace

DecoderBufferAllocator::DecoderBufferAllocator()
    : DecoderBufferAllocator(SbMediaIsBufferPoolAllocateOnDemand(),
                             SbMediaGetInitialBufferCapacity(),
                             SbMediaGetBufferAllocationUnit()) {}

DecoderBufferAllocator::DecoderBufferAllocator(
    bool is_memory_pool_allocated_on_demand,
    int initial_capacity,
    int allocation_unit)
    : is_memory_pool_allocated_on_demand_(is_memory_pool_allocated_on_demand),
      initial_capacity_(initial_capacity),
      allocation_unit_(allocation_unit) {
  DCHECK_GE(initial_capacity_, 0);
  DCHECK_GE(allocation_unit_, 0);

  if (is_memory_pool_allocated_on_demand_) {
    LOG(INFO) << "Allocated decoder buffer pool on demand.";
    return;
  }

  base::AutoLock scoped_lock(mutex_);

#if !BUILDFLAG(COBALT_IS_RELEASE_BUILD)
  // Uncomment the following line to default enable MediaBufferPoolStrategy.
  // media_buffer_pool_strategy_state_ = MediaBufferPoolStrategyState::kEnabled;
#endif  // !BUILDFLAG(COBALT_IS_RELEASE_BUILD)

  EnsureStrategyIsCreated();
}

DecoderBufferAllocator::~DecoderBufferAllocator() {
  base::AutoLock scoped_lock(mutex_);

  if (strategy_) {
    DCHECK_EQ(strategy_->GetAllocated(), 0);
    strategy_.reset();
  }
}

void DecoderBufferAllocator::Suspend() {
  base::AutoLock scoped_lock(mutex_);
  if (is_memory_pool_allocated_on_demand_) {
    return;
  }

  if (strategy_ && strategy_->GetAllocated() == 0) {
    LOG(INFO) << "Freeing " << strategy_->GetCapacity()
              << " bytes of decoder buffer pool `on suspend`.";
    strategy_.reset();
  }
}

void DecoderBufferAllocator::Resume() {
  base::AutoLock scoped_lock(mutex_);
  if (is_memory_pool_allocated_on_demand_) {
    return;
  }

  EnsureStrategyIsCreated();
}

DecoderBuffer::Allocator::Handle DecoderBufferAllocator::Allocate(
    DemuxerStream::Type type,
    size_t size,
    size_t alignment) {
  base::AutoLock scoped_lock(mutex_);

  EnsureStrategyIsCreated();

  void* p = strategy_->Allocate(type, size, alignment);
  CHECK(p);

#if !BUILDFLAG(COBALT_IS_RELEASE_BUILD)
  if (starboard::common::Allocator::ExtraLogLevel() >= 2) {
    ++pending_allocation_operations_count_;
    pending_allocation_operations_ << " a " << p << " " << type << " " << size
                                   << " " << alignment;
    TryFlushAllocationLog_Locked();
  }
#endif  // !BUILDFLAG(COBALT_IS_RELEASE_BUILD)

  return reinterpret_cast<Handle>(p);
}

void DecoderBufferAllocator::Free(DemuxerStream::Type type,
                                  Handle handle,
                                  size_t size) {
  void* p = reinterpret_cast<void*>(handle);

  if (p == nullptr) {
    DCHECK_EQ(size, 0);
    return;
  }

  base::AutoLock scoped_lock(mutex_);

  DCHECK(strategy_);

  strategy_->Free(type, p);

#if !BUILDFLAG(COBALT_IS_RELEASE_BUILD)
  if (starboard::common::Allocator::ExtraLogLevel() >= 2) {
    ++pending_allocation_operations_count_;
    pending_allocation_operations_ << " f " << p;
    TryFlushAllocationLog_Locked();
  }
#endif  // !BUILDFLAG(COBALT_IS_RELEASE_BUILD)

  bool should_reset_strategy =
      media_buffer_pool_strategy_state_ ==
          MediaBufferPoolStrategyState::kPendingEnabling ||
      is_memory_pool_allocated_on_demand_;

  if (should_reset_strategy && strategy_->GetAllocated() == 0) {
    // `strategy_->PrintAllocations()` will be called inside the dtor when
    // supported, so it shouldn't be called here.
    LOG(INFO) << "Freeing " << strategy_->GetCapacity()
              << " bytes of decoder buffer pool.";
    strategy_.reset();
  }
}

void DecoderBufferAllocator::Write(Handle handle,
                                   const void* data,
                                   size_t size) {
  // The lock adds overhead to the cases where |handle| is a pointer, so we take
  // a short cut to ensure that there is no overhead adding to our existing
  // logic.
  using ::starboard::common::experimental::IsPointerAnnotated;

  if (!IsPointerAnnotated(handle)) {
    memcpy(reinterpret_cast<void*>(handle), data, size);
    return;
  }

  // TODO(b/369245553): Consider combining Allocate() and Write() into one
  //                    function to avoid the extra lock.
  base::AutoLock scoped_lock(mutex_);
  DCHECK(strategy_);
  strategy_->Write(reinterpret_cast<void*>(handle), data, size);
}

int DecoderBufferAllocator::GetBufferAlignment() const {
  return sizeof(void*);
}

int DecoderBufferAllocator::GetBufferPadding() const {
  return SbMediaGetBufferPadding();
}

base::TimeDelta
DecoderBufferAllocator::GetBufferGarbageCollectionDurationThreshold() const {
  return base::Microseconds(
      SbMediaGetBufferGarbageCollectionDurationThreshold());
}

size_t DecoderBufferAllocator::GetAllocatedMemory() const {
  base::AutoLock scoped_lock(mutex_);
  return strategy_ ? strategy_->GetAllocated() : 0;
}

size_t DecoderBufferAllocator::GetCurrentMemoryCapacity() const {
  base::AutoLock scoped_lock(mutex_);
  return strategy_ ? strategy_->GetCapacity() : 0;
}

size_t DecoderBufferAllocator::GetMaximumMemoryCapacity() const {
  // Always returns 0, as we no longer cap the capacity since Cobalt 25.
  //
  // base::AutoLock scoped_lock(mutex_);
  return 0;
}

void DecoderBufferAllocator::SetAllocateOnDemand(bool enabled) {
  base::AutoLock scoped_lock(mutex_);
  if (is_memory_pool_allocated_on_demand_ == enabled) {
    return;
  }

  LOG(INFO) << "DecoderBufferAllocator::SetAllocateOnDemand: "
            << ToString(is_memory_pool_allocated_on_demand_) << " -> "
            << ToString(enabled);

  is_memory_pool_allocated_on_demand_ = enabled;
  // If we enable |is_memory_pool_allocated_on_demand_|, we should try to
  // reset the strategy.
  if (is_memory_pool_allocated_on_demand_ && strategy_ &&
      strategy_->GetAllocated() == 0) {
    LOG(INFO) << "Freeing " << strategy_->GetCapacity()
              << " bytes of decoder buffer pool since allocator now allocates"
                 " on demand.";
    strategy_.reset();
  }
}

void DecoderBufferAllocator::EnableMediaBufferPoolStrategy() {
  base::AutoLock scoped_lock(mutex_);

  if (media_buffer_pool_strategy_state_ !=
      MediaBufferPoolStrategyState::kDisabled) {
    return;
  }

  if (strategy_ && strategy_->GetAllocated() > 0) {
    // There is another strategy being used, we have to wait until all
    // allocations are freed before switching to MediaBufferPool based strategy.
    media_buffer_pool_strategy_state_ =
        MediaBufferPoolStrategyState::kPendingEnabling;
    return;
  }

  if (strategy_) {
    strategy_.reset();
  }

  media_buffer_pool_strategy_state_ = MediaBufferPoolStrategyState::kEnabled;
}

void DecoderBufferAllocator::EnsureStrategyIsCreated() {
  mutex_.AssertAcquired();
  if (strategy_) {
    return;
  }

  if (media_buffer_pool_strategy_state_ ==
      MediaBufferPoolStrategyState::kPendingEnabling) {
    media_buffer_pool_strategy_state_ = MediaBufferPoolStrategyState::kEnabled;
  }

  if (media_buffer_pool_strategy_state_ ==
      MediaBufferPoolStrategyState::kEnabled) {
    auto pool = starboard::common::experimental::MediaBufferPool::Acquire();
    if (pool) {
      strategy_.reset(new MediaBufferPoolDecoderBufferAllocatorStrategy(
          pool, initial_capacity_, allocation_unit_));
      LOG(INFO) << "DecoderBufferAllocator is using MediaBufferPool.";
      return;
    } else {
      LOG(INFO) << "DecoderBufferAllocator failed to enable MediaBufferPool as"
                << " MediaBufferPool::Acquire() returns nullptr.";
    }
  }

  if (base::FeatureList::IsEnabled(
          kCobaltDecoderBufferAllocatorWithInPlaceMetadata)) {
    strategy_.reset(new BidirectionalFitDecoderBufferAllocatorStrategy<
                    starboard::common::InPlaceReuseAllocatorBase>(
        initial_capacity_, allocation_unit_));
    LOG(INFO) << "DecoderBufferAllocator is using InPlaceReuseAllocatorBase.";
  } else {
    strategy_.reset(new BidirectionalFitDecoderBufferAllocatorStrategy<
                    starboard::common::ReuseAllocatorBase>(initial_capacity_,
                                                           allocation_unit_));
    LOG(INFO) << "DecoderBufferAllocator is using ReuseAllocatorBase.";
  }

  LOG(INFO) << "Allocated " << initial_capacity_
            << " bytes for decoder buffer pool.";
}

#if !BUILDFLAG(COBALT_IS_RELEASE_BUILD)
void DecoderBufferAllocator::TryFlushAllocationLog_Locked() {
  const int kMaxOperationsPerLog = 80;

  mutex_.AssertAcquired();

  // The allocation operations may generate a few hundred log lines per second
  // and lead to missing entries on some platforms.  Grouping them helps avoid
  // missing entries, and the log index will be verified in the processing
  // script.
  if ((allocation_operation_index_ + pending_allocation_operations_count_) %
              kMaxOperationsPerLog ==
          0 ||
      strategy_->GetAllocated() == 0) {
    SB_LOG(INFO) << " Media Allocation Log: " << allocation_operation_index_
                 << pending_allocation_operations_.str();

    allocation_operation_index_ += pending_allocation_operations_count_;
    pending_allocation_operations_count_ = 0;
    // Reset the pending log
    pending_allocation_operations_.str("");
    pending_allocation_operations_.clear();
  }
}
#endif  // !BUILDFLAG(COBALT_IS_RELEASE_BUILD)

}  // namespace media
