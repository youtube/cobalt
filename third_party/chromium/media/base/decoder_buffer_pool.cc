// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/decoder_buffer_pool.h"

#include "base/containers/circular_deque.h"
// #include "base/functional/bind.h"
#include "base/logging.h"
// #include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/synchronization/lock.h"
// #include "base/thread_annotations.h"
#include "base/time/default_tick_clock.h"
#include "base/time/time.h"

namespace media {

class DecoderBufferPool::PoolImpl
    : public base::RefCountedThreadSafe<DecoderBufferPool::PoolImpl> {
 public:
  PoolImpl();
  PoolImpl(const PoolImpl&) = delete;
  PoolImpl& operator=(const PoolImpl&) = delete;

  scoped_refptr<DecoderBuffer> CopyFrom(const uint8_t* data,
                                               size_t size);

  // Shuts down the buffer pool and releases all buffers in |buffers_|.
  // Once this is called buffers will no longer be inserted back into
  // |buffers_|.
  void Shutdown();

  size_t get_pool_size_for_testing() {
    base::AutoLock auto_lock(lock_);
    return buffers_.size();
  }

  //void set_tick_clock_for_testing(const base::TickClock* tick_clock) {
  //  tick_clock_ = tick_clock;
  //}

 private:
  friend class base::RefCountedThreadSafe<DecoderBufferPool::PoolImpl>;
  ~PoolImpl();

  // Called when the buffer wrapper gets destroyed. |buffer| is the actual buffer
  // that was wrapped and is placed in |buffers_| by this function so it can be
  // reused. This will attempt to expire buffers that haven't been used in some
  // time. It relies on |buffers_| being in LRU order with the front being the
  // least recently used entry.
  void BufferReleased(scoped_refptr<DecoderBuffer> buffer);

  base::Lock lock_;
  bool is_shutdown_ GUARDED_BY(lock_) = false;

  struct BufferEntry {
    base::TimeTicks last_use_time;
    scoped_refptr<DecoderBuffer> buffer;
  };

  base::circular_deque<BufferEntry> buffers_ GUARDED_BY(lock_);

  // |tick_clock_| is always a DefaultTickClock outside of testing.
  //raw_ptr<const base::TickClock> tick_clock_;
};

DecoderBufferPool::PoolImpl::PoolImpl() {}
//    : tick_clock_(base::DefaultTickClock::GetInstance()) {}

DecoderBufferPool::PoolImpl::~PoolImpl() {
  DCHECK(is_shutdown_);
}

scoped_refptr<DecoderBuffer> DecoderBufferPool::PoolImpl::CopyFrom(const uint8_t* data,
                                               size_t size) {
  base::AutoLock auto_lock(lock_);
  DCHECK(!is_shutdown_);

  // // get memory from allocator
  //  int alignment = s_allocator->GetBufferAlignment();
  // int padding = s_allocator->GetBufferPadding();
  // allocated_size_ = size + padding;
  // data_ = static_cast<uint8_t*>(s_allocator->Allocate(allocated_size_,
  //                                                     alignment));
  // // use for creating DecoderBuffer ExternalMemory
  //  constexpr uint8_t kData[] = "hello";
  // constexpr size_t kDataSize = std::size(kData);
  // auto external_memory = std::make_unique<DecoderBuffer::ExternalMemory>(
  //     base::make_span(kData, kDataSize));
  // auto buffer = DecoderBuffer::FromExternalMemory(std::move(external_memory));

  return DecoderBuffer::CopyFrom(data, size);

}

void DecoderBufferPool::PoolImpl::Shutdown() {
  base::AutoLock auto_lock(lock_);
  is_shutdown_ = true;
  buffers_.clear();
}

DecoderBufferPool::DecoderBufferPool() : pool_(new PoolImpl()) {}

DecoderBufferPool::~DecoderBufferPool() {
  pool_->Shutdown();
}

scoped_refptr<DecoderBuffer> DecoderBufferPool::CopyFrom(const uint8_t* data,
                                               size_t size) {
  return pool_->CopyFrom(data, size);
}

size_t DecoderBufferPool::GetPoolSizeForTesting() const {
  return pool_->get_pool_size_for_testing();
}

//  void DecoderBufferPool::SetTickClockForTesting(const base::TickClock* tick_clock) {
//    pool_->set_tick_clock_for_testing(tick_clock);
//  }

}  // namespace media
