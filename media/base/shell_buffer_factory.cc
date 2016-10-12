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

#include "base/debug/trace_event.h"
#include "base/logging.h"
#include "base/stringprintf.h"
#include "media/base/decrypt_config.h"
#include "media/base/shell_media_platform.h"
#include "media/base/shell_media_statistics.h"

namespace media {

// ==== ShellScopedArray =======================================================

ShellScopedArray::ShellScopedArray(uint8* reusable_buffer, size_t size)
    : array_(reusable_buffer), size_(size) {
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

// ==== ShellBufferFactory =====================================================

scoped_refptr<ShellBufferFactory> ShellBufferFactory::instance_ = NULL;

// static
void ShellBufferFactory::Initialize() {
  // safe to call multiple times
  if (!instance_) {
    instance_ = new ShellBufferFactory();
  }
}

bool ShellBufferFactory::AllocateBuffer(size_t size,
                                        bool is_keyframe,
                                        AllocCB cb) {
  TRACE_EVENT1("media_stack", "ShellBufferFactory::AllocateBuffer()", "size",
               size);
  // Zero-size buffers are allocation error, allocate an EOS buffer explicity
  // with the provided EOS method.
  if (size == 0) {
    TRACE_EVENT0("media_stack",
                 "ShellBufferFactory::AllocateBuffer() failed as size is 0.");
    return false;
  }

  // If we can allocate a buffer right now save a pointer to it so that we don't
  // run the callback while holding the memory lock, for safety's sake.
  scoped_refptr<DecoderBuffer> instant_buffer = NULL;

  {
    base::AutoLock lock(lock_);
    // We only service requests directly if there's no callbacks pending and
    // we can accommodate a buffer of the requested size
    if (pending_allocs_.size() == 0) {
      uint8* bytes = Allocate_Locked(size);
      if (bytes) {
        instant_buffer = new DecoderBuffer(bytes, size, is_keyframe);
        TRACE_EVENT0(
            "media_stack",
            "ShellBufferFactory::AllocateBuffer() finished allocation.");
        DCHECK(!instant_buffer->IsEndOfStream());
      }
    }
    if (!instant_buffer) {
      // Alright, we have to wait, enqueue the buffer and size.
      TRACE_EVENT0("media_stack",
                   "ShellBufferFactory::AllocateBuffer() deferred.");
      pending_allocs_.push_back(
          std::make_pair(cb, new DecoderBuffer(NULL, size, is_keyframe)));
    }
  }

  // If we managed to create a buffer run the callback after releasing the lock.
  if (instant_buffer) {
    cb.Run(instant_buffer);
  }
  return true;
}

scoped_refptr<DecoderBuffer> ShellBufferFactory::AllocateBufferNow(
    size_t size,
    bool is_keyframe) {
  TRACE_EVENT1("media_stack", "ShellBufferFactory::AllocateBufferNow()", "size",
               size);
  // Zero-size buffers are allocation error, allocate an EOS buffer explicity
  // with the provided EOS method.
  if (size == 0) {
    TRACE_EVENT0(
        "media_stack",
        "ShellBufferFactory::AllocateBufferNow() failed as size is 0.");
    return NULL;
  }

  base::AutoLock lock(lock_);
  uint8* bytes = Allocate_Locked(size);
  if (!bytes) {
    TRACE_EVENT0(
        "media_stack",
        "ShellBufferFactory::AllocateBufferNow() failed as size is too large.");
    return NULL;
  }
  scoped_refptr<DecoderBuffer> buffer =
      new DecoderBuffer(bytes, size, is_keyframe);
  TRACE_EVENT0("media_stack",
               "ShellBufferFactory::AllocateBufferNow() finished allocation.");
  DCHECK(!buffer->IsEndOfStream());

  return buffer;
}

uint8* ShellBufferFactory::AllocateNow(size_t size) {
  // we skip to the head of the line for these allocations, if there's
  // room we allocate it.
  base::AutoLock lock(lock_);
  uint8* bytes = Allocate_Locked(size);

  if (!bytes) {
    DLOG(ERROR) << base::StringPrintf("Failed to allocate %d bytes!",
                                      (int)size);
  }

  return bytes;
}

scoped_refptr<ShellScopedArray> ShellBufferFactory::AllocateArray(size_t size) {
  TRACE_EVENT1("media_stack", "ShellBufferFactory::AllocateArray()", "size",
               size);
  uint8* allocated_bytes = NULL;
  if (size == 0) {
    TRACE_EVENT0("media_stack",
                 "ShellBufferFactory::AllocateArray() failed as size is 0.");
    return NULL;
  }

  if (size <= kShellMaxArraySize) {
    base::AutoLock lock(lock_);
    // there should not already be somebody waiting on an array
    if (array_requested_size_ > 0) {
      TRACE_EVENT0(
          "media_stack",
          "ShellBufferFactory::AllocateArray() failed as another allocation is"
          " in progress.");
      NOTREACHED() << "Max one thread blocking on array allocation at a time.";
      return NULL;
    }
    // Attempt to allocate.
    allocated_bytes = Allocate_Locked(size);
    // If we don't have room save state while we still have the lock
    if (!allocated_bytes) {
      array_requested_size_ = size;
    }
  } else {  // oversized requests always fail instantly.
    TRACE_EVENT0(
        "media_stack",
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
      DCHECK_EQ(size, array_requested_size_);
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
  FinishList finished_allocs;

  // Reclaim() on a NULL buffer is a no-op, don't even acquire the lock.
  if (p) {
    base::AutoLock lock(lock_);
    ShellMediaPlatform::Instance()->FreeBuffer(p);

    // Try to service a blocking array request if there is one, and it hasn't
    // already been serviced. If we can't service it then we won't allocate any
    // additional ShellBuffers as arrays get priority treatment.
    bool service_buffers = true;
    if (array_requested_size_ > 0 && !array_allocation_) {
      array_allocation_ = Allocate_Locked(array_requested_size_);
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
      uint8* bytes = Allocate_Locked(size);
      if (bytes) {
        scoped_refptr<DecoderBuffer> alloc_buff =
            pending_allocs_.front().second;
        alloc_buff->SetBuffer(bytes);
        TRACE_EVENT1("media_stack",
                     "ShellBufferFactory::Reclaim() finished allocation.",
                     "size", size);
        finished_allocs.push_back(
            std::make_pair(pending_allocs_.front().first, alloc_buff));
        pending_allocs_.pop_front();
      } else {
        service_buffers = false;
      }
    }
  }
  // OK, lock released, do callbacks for finished allocs
  for (FinishList::iterator it = finished_allocs.begin();
       it != finished_allocs.end(); ++it) {
    it->first.Run(it->second);
  }
}

uint8* ShellBufferFactory::Allocate_Locked(size_t size) {
  // should have acquired the lock already
  lock_.AssertAcquired();
  UPDATE_MEDIA_STATISTICS(STAT_TYPE_ALLOCATED_SHELL_BUFFER_SIZE, size);
  return static_cast<uint8*>(
      ShellMediaPlatform::Instance()->AllocateBuffer(size));
}

// static
void ShellBufferFactory::Terminate() {
  instance_ = NULL;
}

ShellBufferFactory::ShellBufferFactory()
    : array_allocation_event_(false, false),
      array_requested_size_(0),
      array_allocation_(NULL) {}

// Will be called when all ShellBuffers have been deleted AND instance_ has
// been set to NULL.
ShellBufferFactory::~ShellBufferFactory() {
  // and no outstanding array requests
  DCHECK_EQ(array_requested_size_, 0);
}

}  // namespace media
