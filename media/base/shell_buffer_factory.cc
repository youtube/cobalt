/*
 * Copyright 2012 Google Inc. All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "shell_buffer_factory.h"

#include <malloc.h> // for memalign

#include "base/logging.h"
#include "base/stringprintf.h"
#include "media/base/decrypt_config.h"
#include "media/base/shell_filter_graph_log.h"
#include "media/base/shell_filter_graph_log_constants.h"

namespace media {

// ==== ShellScopedArray =======================================================

ShellScopedArray::ShellScopedArray(
    uint8* reusable_buffer,
    size_t size,
    scoped_refptr<ShellFilterGraphLog> filter_graph_log)
    : array_(reusable_buffer)
    , size_(size)
    , filter_graph_log_(filter_graph_log) {
}

ShellScopedArray::~ShellScopedArray() {
  if (array_) {
    filter_graph_log_->LogEvent(kObjectIdBufferFactory,
                                kEventArrayAllocationReclaim);
    ShellBufferFactory::Instance()->Reclaim(array_);
  }
}

scoped_refptr<ShellFilterGraphLog> ShellScopedArray::filter_graph_log() {
  return filter_graph_log_;
}

// ==== ShellBuffer ============================================================

// static
scoped_refptr<ShellBuffer> ShellBuffer::CreateEOSBuffer(
    base::TimeDelta timestamp,
    scoped_refptr<ShellFilterGraphLog> filter_graph_log) {
 scoped_refptr<ShellBuffer> eos = scoped_refptr<ShellBuffer>(
    new ShellBuffer(NULL, 0, filter_graph_log));
 eos->SetTimestamp(timestamp);
 return eos;
}

void ShellBuffer::ShrinkTo(int size) {
  CHECK_LE(size, GetAllocatedSize());
  size_ = size;
}

ShellBuffer::ShellBuffer(uint8* reusable_buffer,
                         size_t size,
                         scoped_refptr<ShellFilterGraphLog> filter_graph_log)
    : Buffer(kNoTimestamp(), kInfiniteDuration())
    , buffer_(reusable_buffer)
    , size_(size)
    , allocated_size_(size)
    , filter_graph_log_(filter_graph_log) {
}

ShellBuffer::~ShellBuffer() {
  // recycle our buffer
  if (buffer_) {
    filter_graph_log_->LogEvent(kObjectIdBufferFactory,
                                kEventBufferAllocationReclaim);
    ShellBufferFactory::Instance()->Reclaim(buffer_);
  }
}

scoped_refptr<ShellFilterGraphLog> ShellBuffer::filter_graph_log() {
  return filter_graph_log_;
}

const DecryptConfig* ShellBuffer::GetDecryptConfig() const {
  DCHECK(!IsEndOfStream());
  return decrypt_config_.get();
}

void ShellBuffer::SetDecryptConfig(scoped_ptr<DecryptConfig> decrypt_config) {
  DCHECK(!IsEndOfStream());
  decrypt_config_ = decrypt_config.Pass();
}

// ==== ShellBufferFactory =====================================================

scoped_refptr<ShellBufferFactory> ShellBufferFactory::instance_ = NULL;

// static
void ShellBufferFactory::Initialize() {
  // safe to call multiple times
  if (!instance_) {
    instance_ = new ShellBufferFactory();
  }
}

bool ShellBufferFactory::AllocateBuffer(
    size_t size,
    AllocCB cb,
    scoped_refptr<ShellFilterGraphLog> filter_graph_log) {
  filter_graph_log->LogEvent(kObjectIdBufferFactory,
                             kEventBufferAllocationRequest);
  // Zero-size buffers are allocation error, allocate an EOS buffer explicity
  // with the provided EOS method.
  if (size == 0) {
    filter_graph_log->LogEvent(kObjectIdBufferFactory,
                               kEventBufferAllocationError);
    return false;
  }
  size_t aligned_size = SizeAlign(size);
  // If we can allocate a buffer right now save a pointer to it so that we don't
  // run the callback while holding the memory lock, for safety's sake.
  scoped_refptr<ShellBuffer> instant_buffer = NULL;
  if (aligned_size <= kShellMaxBufferSize) {
    base::AutoLock lock(lock_);
    // We only service requests directly if there's no callbacks pending and
    // we can accommodate a buffer of the requested size
    uint8* shell_buffer_bytes;
    if (pending_allocs_.size() == 0 && aligned_size <= largest_free_space_) {
      instant_buffer = new ShellBuffer(AllocateLockAcquired(aligned_size),
                                       size,
                                       filter_graph_log);
      filter_graph_log->LogEvent(kObjectIdBufferFactory,
                                 kEventBufferAllocationSuccess);
      DCHECK(!instant_buffer->IsEndOfStream());
    } else {
      // Alright, we have to wait, enqueue the buffer and size.
      DLOG(INFO) << base::StringPrintf(
          "deferred allocation of %d bytes", size);
      filter_graph_log->LogEvent(kObjectIdBufferFactory,
                                 kEventBufferAllocationDeferred);
      pending_allocs_.push_back(
          std::make_pair(cb, new ShellBuffer(NULL, size, filter_graph_log)));
    }
  } else {
    // allocation size request was too large, return failure
    filter_graph_log->LogEvent(kObjectIdBufferFactory,
                               kEventBufferAllocationError);
    return false;
  }
  // If we managed to create a buffer run the callback after releasing the lock.
  if (instant_buffer) {
    cb.Run(instant_buffer);
  }
  return true;
}

bool ShellBufferFactory::HasRoomForBufferNow(size_t size) {
  base::AutoLock lock(lock_);
  return (SizeAlign(size) <= largest_free_space_);
}

uint8* ShellBufferFactory::AllocateNow(size_t size) {
  size_t aligned_size = SizeAlign(size);
  uint8* bytes = NULL;
  // we skip to the head of the line for these allocations, if there's
  // room we allocate it.
  base::AutoLock lock(lock_);
  if (aligned_size <= largest_free_space_) {
    bytes = AllocateLockAcquired(aligned_size);
    DCHECK(bytes);
  }

  if (!bytes) {
    DLOG(ERROR) << base::StringPrintf("Failed to allocate %d bytes!", size);
  }

  return bytes;
}

scoped_refptr<ShellScopedArray> ShellBufferFactory::AllocateArray(
    size_t size,
    scoped_refptr<ShellFilterGraphLog> filter_graph_log) {
  filter_graph_log->LogEvent(kObjectIdBufferFactory,
                             kEventArrayAllocationRequest);
  uint8* allocated_bytes = NULL;
  if (size == 0) {
    filter_graph_log->LogEvent(kObjectIdBufferFactory,
                               kEventArrayAllocationError);
    return NULL;
  }
  size_t aligned_size = SizeAlign(size);
  if (aligned_size <= kShellMaxArraySize) {
    base::AutoLock lock(lock_);
    // there should not already be somebody waiting on an array
    if (array_requested_size_ > 0) {
      filter_graph_log->LogEvent(kObjectIdBufferFactory,
                                 kEventArrayAllocationError);
      NOTREACHED() << "Max one thread blocking on array allocation at a time.";
      return NULL;
    }
    // Attempt to allocate.
    allocated_bytes = AllocateLockAcquired(aligned_size);
    // If we don't have room save state while we still have the lock
    if (!allocated_bytes) {
      array_requested_size_ = aligned_size;
    }
  } else {  // oversized requests always fail instantly.
    filter_graph_log->LogEvent(kObjectIdBufferFactory,
                               kEventArrayAllocationError);
    return NULL;
  }
  // Lock is released. Now safe to block this thread if we need to.
  if (!allocated_bytes) {
    filter_graph_log->LogEvent(kObjectIdBufferFactory,
                               kEventArrayAllocationDeferred);
    // Wait until enough memory has been released to service this allocation.
    array_allocation_event_.Wait();
    {
      // acquire lock to get address and clear requested size
      base::AutoLock lock(lock_);
      // make sure this allocation makes sense
      DCHECK_EQ(aligned_size, array_requested_size_);
      DCHECK(array_allocation_);
      allocated_bytes = array_allocation_;
      array_allocation_ = NULL;
      array_requested_size_ = 0;
    }
  }
  // Whether we blocked or not we should now have a pointer
  DCHECK(allocated_bytes);
  filter_graph_log->LogEvent(kObjectIdBufferFactory,
                             kEventArrayAllocationSuccess);
  return scoped_refptr<ShellScopedArray>(
      new ShellScopedArray(allocated_bytes, size, filter_graph_log));
}

void ShellBufferFactory::Reclaim(uint8* p) {
  typedef std::list<std::pair<AllocCB, scoped_refptr<ShellBuffer> > >
      FinishList;
  FinishList finished_allocs_;

  // Reclaim() on a NULL buffer is a no-op, don't even acquire the lock.
  if (p) {
    base::AutoLock lock(lock_);
    AllocMap::iterator p_it = allocs_.find(p);
    // sanity-check that this offset is indeed in the map
    if (p_it == allocs_.end()) {
      NOTREACHED();
      return;
    }
#if defined(_DEBUG)
    // scribble recycled memory in debug builds with 0xef
    memset(p, 0xef, p_it->second);
#endif
    // remove from the map
    allocs_.erase(p_it);
    // update memory block info
    RecalculateLargestFreeSpaceLockAcquired();
    // Try to service a blocking array request if there is one, and it hasn't
    // already been serviced. If we can't service it then we won't allocate any
    // additional ShellBuffers as arrays get priority treatment.
    bool service_buffers = true;
    if (array_requested_size_ > 0 && !array_allocation_) {
      array_allocation_ = AllocateLockAcquired(array_requested_size_);
      if (array_allocation_) {
        // Wake up blocked thread
        array_allocation_event_.Signal();
      } else {
        // Not enough room for the array so don't give away what room we have
        // to the buffers.
        service_buffers = false;
      }
    }
    // Try to process any enqueued allocs in FIFO order until we run out of room
    while (service_buffers && pending_allocs_.size()) {
      size_t size = pending_allocs_.front().second->GetAllocatedSize();
      size_t aligned_size = SizeAlign(size);
      uint8* bytes = AllocateLockAcquired(aligned_size);
      if (bytes) {
        scoped_refptr<ShellBuffer> alloc_buff = pending_allocs_.front().second;
        alloc_buff->SetBuffer(bytes);
        alloc_buff->filter_graph_log()->LogEvent(kObjectIdBufferFactory,
                                                 kEventBufferAllocationSuccess);
        finished_allocs_.push_back(std::make_pair(
            pending_allocs_.front().first, alloc_buff));
        pending_allocs_.pop_front();
      } else {
        service_buffers = false;
      }
    }
  }
  // OK, lock released, do callbacks for finished allocs
  for (FinishList::iterator it = finished_allocs_.begin();
       it != finished_allocs_.end(); ++it) {
    it->first.Run(it->second);
  }
}

uint8* ShellBufferFactory::AllocateLockAcquired(size_t aligned_size) {
  // should have acquired the lock already
  lock_.AssertAcquired();
  // and should have aligned the size already
  DCHECK_EQ(aligned_size % kShellBufferAlignment, 0);
  // Quick check that we have a reasonable expectation of fitting this size.
  if (aligned_size > largest_free_space_) {
    return NULL;
  }
  // find the best fit for this buffer
  uint8* best_fit = buffer_;
  size_t best_fit_size = kShellBufferSpaceSize;
  uint8* last_alloc_end = buffer_;
  for (AllocMap::iterator it = allocs_.begin(); it != allocs_.end(); it++) {
    size_t free_block_size = it->first - last_alloc_end;
    if (free_block_size >= aligned_size && free_block_size < best_fit_size) {
      best_fit_size = free_block_size;
      best_fit = last_alloc_end;
    }
    last_alloc_end = it->first + it->second;
  }
  // should have found a block that fits within at least the largest free space
  // and also within our shared buffer overall
  DCHECK_LE(best_fit_size, largest_free_space_) <<
      "found a free block larger than largest_free_space";
  DCHECK_LE(best_fit + aligned_size - buffer_, kShellBufferSpaceSize);
  // save this new allocation in the map
  allocs_[best_fit] = aligned_size;
  // if we broke up the largest free space, recalculate it
  if (best_fit_size == largest_free_space_) {
    RecalculateLargestFreeSpaceLockAcquired();
  }
  return best_fit;
}

void ShellBufferFactory::RecalculateLargestFreeSpaceLockAcquired() {
  // should have already acquired the lock
  lock_.AssertAcquired();
  // recalculate largest free space after allocation or release
  largest_free_space_ = 0;
  uint8* last_alloc_end = buffer_;
  for (AllocMap::iterator it = allocs_.begin(); it != allocs_.end(); it++) {
    // last_alloc_end describes the lowest byte-aligned value past the size of
    // the last alloc, which is the lowest address that a new alloc could be
    // created at. We compare this to the starting offset of this alloc in the
    // shared memory space, as this defines the byte range of free space.
    largest_free_space_ = std::max(largest_free_space_,
                                   (size_t)(it->first - last_alloc_end));
    last_alloc_end = it->first + it->second;
  }
}

// static
void ShellBufferFactory::Terminate() {
  instance_ = NULL;
}

ShellBufferFactory::ShellBufferFactory()
    : largest_free_space_(kShellBufferSpaceSize)
    , array_allocation_event_(false, false)
    , array_requested_size_(0)
    , array_allocation_(NULL) {
  buffer_ = (uint8*)memalign(kShellBufferAlignment, kShellBufferSpaceSize);
  // we insert a guard alloc at the first byte after the end of the buffer
  // space, to simplify the logic surrounding free space calculations
  allocs_[buffer_ + kShellBufferSpaceSize] = 0;
}

// Will be called when all ShellBuffers have been deleted AND instance_ has
// been set to NULL.
ShellBufferFactory::~ShellBufferFactory() {
  // should be nothing but the guard alloc remaining in the alloc_ map
  if (allocs_.size() > 1) {
    DLOG(WARNING) << base::StringPrintf(
        "%d unfreed allocs on termination now pointing at invalid memory!",
        static_cast<int>(allocs_.size()) - 1);
  }
  // and no outstanding array requests
  DCHECK_EQ(array_requested_size_, 0);
  free(buffer_);
}


}  // namespace media
