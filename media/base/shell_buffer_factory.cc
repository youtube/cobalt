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

#include "media/base/shell_buffer_factory.h"

#include <malloc.h> // for memalign

#include "base/debug/trace_event.h"
#include "base/logging.h"
#include "base/stringprintf.h"
#include "media/base/decrypt_config.h"
#include "media/base/shell_media_platform.h"
#include "media/base/shell_media_statistics.h"

namespace {

// Returns the provided size aligned to kShellBufferAlignment.
static inline size_t SizeAlign(size_t size) {
  size_t align =
      media::ShellMediaPlatform::Instance()->GetShellBufferSpaceAlignment();
  return ((size + align - 1) / align) * align;
}

}  // namespace

namespace media {

// ==== ShellScopedArray =======================================================

ShellScopedArray::ShellScopedArray(
    uint8* reusable_buffer,
    size_t size)
    : array_(reusable_buffer)
    , size_(size) {
  if (array_) {
    // Retain a reference to the buffer factory, to ensure that we do not
    // outlive it.
    buffer_factory_ = ShellBufferFactory::Instance();
  }
}

ShellScopedArray::~ShellScopedArray() {
  TRACE_EVENT0("media_stack", "ShellScopedArray::~ShellScopedArray()");
  if (array_) {
    buffer_factory_->Reclaim(array_);
  }
}

// ==== DecoderBuffer =========================================================

// static
scoped_refptr<DecoderBuffer> DecoderBuffer::CreateEOSBuffer(
    base::TimeDelta timestamp) {
  scoped_refptr<DecoderBuffer> eos = scoped_refptr<DecoderBuffer>(
    new DecoderBuffer(NULL, 0));
  eos->SetTimestamp(timestamp);
  return eos;
}

void DecoderBuffer::ShrinkTo(int size) {
  CHECK_LE(size, GetAllocatedSize());
  size_ = size;
}

DecoderBuffer::DecoderBuffer(uint8* reusable_buffer,
                             size_t size)
    : Buffer(kNoTimestamp(), kInfiniteDuration())
    , buffer_(reusable_buffer)
    , size_(size)
    , allocated_size_(size)
    , is_decrypted_(false) {
  if (buffer_) {
    // Retain a reference to the buffer factory, to ensure that we do not
    // outlive it.
    buffer_factory_ = ShellBufferFactory::Instance();
  }
}

DecoderBuffer::~DecoderBuffer() {
  // recycle our buffer
  if (buffer_) {
    TRACE_EVENT1("media_stack", "DecoderBuffer::~DecoderBuffer()",
                 "timestamp", GetTimestamp().InMicroseconds());
    DCHECK_NE(buffer_factory_, (ShellBufferFactory*)NULL);
    buffer_factory_->Reclaim(buffer_);
  }
}

const DecryptConfig* DecoderBuffer::GetDecryptConfig() const {
  DCHECK(!IsEndOfStream());
  return decrypt_config_.get();
}

void DecoderBuffer::SetDecryptConfig(scoped_ptr<DecryptConfig> decrypt_config) {
  DCHECK(!IsEndOfStream());
  decrypt_config_ = decrypt_config.Pass();
}

void DecoderBuffer::SetBuffer(uint8* reusable_buffer) {
  buffer_ = reusable_buffer;
  if (buffer_) {
    // Retain a reference to the buffer factory, to ensure that we do not
    // outlive it.
    buffer_factory_ = ShellBufferFactory::Instance();
  }
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
    AllocCB cb) {
  TRACE_EVENT1("media_stack", "ShellBufferFactory::AllocateBuffer()",
               "size", size);
  // Zero-size buffers are allocation error, allocate an EOS buffer explicity
  // with the provided EOS method.
  if (size == 0) {
    TRACE_EVENT0("media_stack",
                 "ShellBufferFactory::AllocateBuffer() failed as size is 0.");
    return false;
  }
  size_t aligned_size = SizeAlign(size);
  // If we can allocate a buffer right now save a pointer to it so that we don't
  // run the callback while holding the memory lock, for safety's sake.
  scoped_refptr<DecoderBuffer> instant_buffer = NULL;

  {
    base::AutoLock lock(lock_);
    // We only service requests directly if there's no callbacks pending and
    // we can accommodate a buffer of the requested size
    uint8* shell_buffer_bytes;
    if (pending_allocs_.size() == 0 &&
        aligned_size <= LargestFreeSpace_Locked()) {
      instant_buffer = new DecoderBuffer(AllocateLockAcquired(aligned_size),
                                         size);
      TRACE_EVENT0("media_stack",
                   "ShellBufferFactory::AllocateBuffer() finished allocation.");
      DCHECK(!instant_buffer->IsEndOfStream());
    } else {
      // Alright, we have to wait, enqueue the buffer and size.
      TRACE_EVENT0("media_stack",
                   "ShellBufferFactory::AllocateBuffer() deferred.");
      pending_allocs_.push_back(
          std::make_pair(cb, new DecoderBuffer(NULL, size)));
    }
  }

  // If we managed to create a buffer run the callback after releasing the lock.
  if (instant_buffer) {
    cb.Run(instant_buffer);
  }
  return true;
}

bool ShellBufferFactory::HasRoomForBufferNow(size_t size) {
  base::AutoLock lock(lock_);
  return (SizeAlign(size) <= LargestFreeSpace_Locked());
}

scoped_refptr<DecoderBuffer> ShellBufferFactory::AllocateBufferNow(
    size_t size) {
  TRACE_EVENT1("media_stack", "ShellBufferFactory::AllocateBufferNow()",
               "size", size);
  // Zero-size buffers are allocation error, allocate an EOS buffer explicity
  // with the provided EOS method.
  if (size == 0) {
    TRACE_EVENT0("media_stack",
        "ShellBufferFactory::AllocateBufferNow() failed as size is 0.");
    return NULL;
  }
  size_t aligned_size = SizeAlign(size);
  base::AutoLock lock(lock_);
  if (aligned_size > LargestFreeSpace_Locked()) {
    TRACE_EVENT0("media_stack",
        "ShellBufferFactory::AllocateBufferNow() failed as size is too large.");
    return NULL;
  }
  scoped_refptr<DecoderBuffer> buffer =
      new DecoderBuffer(AllocateLockAcquired(aligned_size), size);
  TRACE_EVENT0("media_stack",
               "ShellBufferFactory::AllocateBufferNow() finished allocation.");
  DCHECK(!buffer->IsEndOfStream());

  return buffer;
}

uint8* ShellBufferFactory::AllocateNow(size_t size) {
  size_t aligned_size = SizeAlign(size);
  uint8* bytes = NULL;
  // we skip to the head of the line for these allocations, if there's
  // room we allocate it.
  base::AutoLock lock(lock_);
  if (aligned_size <= LargestFreeSpace_Locked()) {
    bytes = AllocateLockAcquired(aligned_size);
    DCHECK(bytes);
  }

  if (!bytes) {
    DLOG(ERROR) << base::StringPrintf("Failed to allocate %d bytes!",
                                      (int)size);
  }

  return bytes;
}

scoped_refptr<ShellScopedArray> ShellBufferFactory::AllocateArray(size_t size) {
  TRACE_EVENT1("media_stack", "ShellBufferFactory::AllocateArray()",
               "size", size);
  uint8* allocated_bytes = NULL;
  if (size == 0) {
    TRACE_EVENT0("media_stack",
                 "ShellBufferFactory::AllocateArray() failed as size is 0.");
    return NULL;
  }
  size_t aligned_size = SizeAlign(size);
  if (aligned_size <= kShellMaxArraySize) {
    base::AutoLock lock(lock_);
    // there should not already be somebody waiting on an array
    if (array_requested_size_ > 0) {
      TRACE_EVENT0("media_stack",
          "ShellBufferFactory::AllocateArray() failed as another allocation is"
          " in progress.");
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
    TRACE_EVENT0("media_stack",
        "ShellBufferFactory::AllocateArray() failed as size is too large.");
    return NULL;
  }
  // Lock is released. Now safe to block this thread if we need to.
  if (!allocated_bytes) {
    TRACE_EVENT0("media_stack",
                 "ShellBufferFactory::AllocateArray() deferred.");
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
  TRACE_EVENT0("media_stack",
               "ShellBufferFactory::AllocateArray() finished allocation.");
  return scoped_refptr<ShellScopedArray>(
      new ShellScopedArray(allocated_bytes, size));
}

void ShellBufferFactory::Reclaim(uint8* p) {
  TRACE_EVENT0("media_stack", "ShellBufferFactory::Reclaim()");
  typedef std::list<std::pair<AllocCB, scoped_refptr<DecoderBuffer> > >
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
    // We need to remove this address,size pair from the alloc map, but before
    // doing so, we examine to the left (lower address) and right (higher addy)
    // to see if there are holes there that can be merged into this hole.
    AllocMap::iterator alloc_left = p_it;
    uint8* hole_addr = NULL;
    size_t left_hole_size = 0;
    if (alloc_left != allocs_.begin()) {
      // decrement to point to the previous alloc in the alloc map
      alloc_left--;
      // if there is a hole before this alloc we are freeing, it will start at
      // the end of the left allocation and will end at the freed alloc
      hole_addr = alloc_left->first + alloc_left->second;
      left_hole_size = p - hole_addr;
    } else {
      // this was the first alloc in the map, so if there's a hole it will start
      // at buffer_ and will have size as distance of this alloc from buffer_
      hole_addr = buffer_;
      left_hole_size = p - buffer_;
    }
    // if there was a hole to our left we must find the size,addr pair with
    // value of the address of that hole, to replace it with an expanded hole
    // welded to the memory we are reclaiming.
    if (left_hole_size) {
      FillHole_Locked(left_hole_size, hole_addr);
    } else {
      // in the case of a contiguous allocation to the left, meaning no hole,
      // hole_addr should point at the buffer being freed
      DCHECK_EQ(hole_addr, p);
    }
    // now we check the allocation to our right, to see if there was a hole there
    // that we could also erase
    AllocMap::iterator alloc_right = p_it;
    alloc_right++;
    size_t right_hole_size = 0;
    uint8* right_hole_addr = p_it->first + p_it->second;
    // this could have been the last allocation in the map
    if (alloc_right != allocs_.end()) {
      // size of right hole is distance between start of the next alloc and
      // end of this alloc.
      right_hole_size = alloc_right->first - right_hole_addr;
    } else {
      // size of right hole is distance between end of the whole buffer_ and
      // the end of this alloc
      right_hole_size = buffer_ +
          ShellMediaPlatform::Instance()->GetShellBufferSpaceSize() -
          right_hole_addr;
    }
    // if there was a hole to our right we remove it from the map and add its
    // size to the final hole
    if (right_hole_size) {
      FillHole_Locked(right_hole_size, right_hole_addr);
    }
    // create a hole potentially merging left and right holes if there
    holes_.insert(
        std::make_pair(left_hole_size + p_it->second + right_hole_size,
                       hole_addr));
    // finally remove freed buffer from the allocs map
    allocs_.erase(p_it);

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
        scoped_refptr<DecoderBuffer> alloc_buff =
            pending_allocs_.front().second;
        alloc_buff->SetBuffer(bytes);
        TRACE_EVENT1("media_stack",
                     "ShellBufferFactory::Reclaim() finished allocation.",
                     "size", size);
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

void ShellBufferFactory::FillHole_Locked(size_t size, uint8* address) {
  lock_.AssertAcquired();
  // do the linear search for holes of this size
  std::pair<HoleMap::iterator, HoleMap::iterator> hole_range;
  hole_range = holes_.equal_range(size);
  HoleMap::iterator hole = hole_range.first;
  for (; hole != hole_range.second; ++hole) {
    if (hole->second == address) {
      break;
    }
  }
  DCHECK_EQ(hole->second, address);
  holes_.erase(hole);
}

size_t ShellBufferFactory::LargestFreeSpace_Locked() const {
  // should have acquired the lock already
  lock_.AssertAcquired();
  if (holes_.empty())
    return 0;

  return holes_.rbegin()->first;
}

uint8* ShellBufferFactory::AllocateLockAcquired(size_t aligned_size) {
  // should have acquired the lock already
  lock_.AssertAcquired();
  // and should have aligned the size already
  DCHECK_EQ(aligned_size %
            ShellMediaPlatform::Instance()->GetShellBufferSpaceAlignment(), 0);
  // Quick check that we have a reasonable expectation of fitting this size.
  if (aligned_size > LargestFreeSpace_Locked()) {
    return NULL;
  }
  // lower_bound() will return the first element >= aligned_size
  HoleMap::iterator best_fit_hole = holes_.lower_bound(aligned_size);
  // since we already checked that there is a hole that this can fit in, this
  // search should have returned a result
  if (best_fit_hole == holes_.end()) {
    NOTREACHED();
    return NULL;
  }
  // copy out values from iterator
  uint8* best_fit_ptr = best_fit_hole->second;
  size_t best_fit_size = best_fit_hole->first;
  // erase hole from map
  holes_.erase(best_fit_hole);
  // if allocating this will create a smaller hole, insert it in to the map
  if (best_fit_size > aligned_size) {
    holes_.insert(
        std::make_pair(best_fit_size - aligned_size,
                       best_fit_ptr + aligned_size));
  }
  // add this allocation to our map
  allocs_.insert(std::make_pair(best_fit_ptr, aligned_size));
  UPDATE_MEDIA_STATISTICS(STAT_TYPE_LARGEST_FREE_SHELL_BUFFER,
                          LargestFreeSpace_Locked());
  UPDATE_MEDIA_STATISTICS(STAT_TYPE_ALLOCATED_SHELL_BUFFER_SIZE, aligned_size);
  return best_fit_ptr;
}

// static
void ShellBufferFactory::Terminate() {
  instance_ = NULL;
}

ShellBufferFactory::ShellBufferFactory()
    : array_allocation_event_(false, false)
    , array_requested_size_(0)
    , array_allocation_(NULL) {
  buffer_ = ShellMediaPlatform::Instance()->GetShellBufferSpace();
  // save the entirety of the available memory as a hole
  holes_.insert(std::make_pair(
      ShellMediaPlatform::Instance()->GetShellBufferSpaceSize(), buffer_));
}

// Will be called when all ShellBuffers have been deleted AND instance_ has
// been set to NULL.
ShellBufferFactory::~ShellBufferFactory() {
  // should be nothing remaining in the alloc_ map
  if (allocs_.size()) {
    DLOG(WARNING) << base::StringPrintf(
        "%d unfreed allocs on termination now pointing at invalid memory!",
        static_cast<int>(allocs_.size()));
  }
  // and no outstanding array requests
  DCHECK_EQ(array_requested_size_, 0);
}

}  // namespace media
