// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/stream_parser_buffer_pool.h"

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

class StreamParserBufferPool::PoolImpl
    : public base::RefCountedThreadSafe<StreamParserBufferPool::PoolImpl> {
 public:
  PoolImpl();
  PoolImpl(const PoolImpl&) = delete;
  PoolImpl& operator=(const PoolImpl&) = delete;

  scoped_refptr<StreamParserBuffer> CopyFrom(const uint8_t* data,
                                               size_t data_size,
                                               bool is_key_frame,
                                               Type type,
                                               TrackId track_id);

  scoped_refptr<StreamParserBuffer> CopyFrom(const uint8_t* data,
                                               size_t data_size,
                                               const uint8_t* side_data,
                                                    int side_data_size,
                                               bool is_key_frame,
                                               Type type,
                                               TrackId track_id);

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
  friend class base::RefCountedThreadSafe<StreamParserBufferPool::PoolImpl>;
  ~PoolImpl();

  // Called when the buffer wrapper gets destroyed. |buffer| is the actual buffer
  // that was wrapped and is placed in |buffers_| by this function so it can be
  // reused. This will attempt to expire buffers that haven't been used in some
  // time. It relies on |buffers_| being in LRU order with the front being the
  // least recently used entry.
  void BufferReleased(scoped_refptr<StreamParserBuffer> buffer);

  base::Lock lock_;
  bool is_shutdown_ GUARDED_BY(lock_) = false;

  struct BufferEntry {
    base::TimeTicks last_use_time;
    scoped_refptr<StreamParserBuffer> buffer;
  };

  base::circular_deque<BufferEntry> buffers_ GUARDED_BY(lock_);

  // |tick_clock_| is always a DefaultTickClock outside of testing.
  //raw_ptr<const base::TickClock> tick_clock_;
};

StreamParserBufferPool::PoolImpl::PoolImpl() {}
//    : tick_clock_(base::DefaultTickClock::GetInstance()) {}

StreamParserBufferPool::PoolImpl::~PoolImpl() {
  DCHECK(is_shutdown_);
}

scoped_refptr<StreamParserBuffer> StreamParserBufferPool::PoolImpl::CopyFrom(const uint8_t* data,
                                               size_t data_size,
                                               bool is_key_frame,
                                               Type type,
                                               TrackId track_id) {
  base::AutoLock auto_lock(lock_);
  DCHECK(!is_shutdown_);

  LOG(INFO) << "YO THOR - STREAM PARSER BUFFER POOL IMPL COPY FROM - 5";
  // // get memory from allocator
  //  int alignment = s_allocator->GetBufferAlignment();
  // int padding = s_allocator->GetBufferPadding();
  // allocated_size_ = size + padding;
  // data_ = static_cast<uint8_t*>(s_allocator->Allocate(allocated_size_,
  //                                                     alignment));
  // // use for creating StreamParser ExternalMemory
  //  constexpr uint8_t kData[] = "hello";
  // constexpr size_t kDataSize = std::size(kData);
  // auto external_memory = std::make_unique<StreamParser::ExternalMemory>(
  //     base::make_span(kData, kDataSize));
  // auto buffer = StreamParser::FromExternalMemory(std::move(external_memory));

  return StreamParserBuffer::CopyFrom(data, data_size,
                                               is_key_frame,
                                               type,
                                               track_id);

}

scoped_refptr<StreamParserBuffer> StreamParserBufferPool::PoolImpl::CopyFrom(const uint8_t* data,
                                               size_t data_size,
                                               const uint8_t* side_data,
                                                    int side_data_size,
                                               bool is_key_frame,
                                               Type type,
                                               TrackId track_id) {
  base::AutoLock auto_lock(lock_);
  DCHECK(!is_shutdown_);

  LOG(INFO) << "YO THOR - STREAM PARSER BUFFER POOL IMPL COPY FROM - 7";
  // // get memory from allocator
  //  int alignment = s_allocator->GetBufferAlignment();
  // int padding = s_allocator->GetBufferPadding();
  // allocated_size_ = size + padding;
  // data_ = static_cast<uint8_t*>(s_allocator->Allocate(allocated_size_,
  //                                                     alignment));
  // // use for creating StreamParser ExternalMemory
  //  constexpr uint8_t kData[] = "hello";
  // constexpr size_t kDataSize = std::size(kData);
  // auto external_memory = std::make_unique<StreamParser::ExternalMemory>(
  //     base::make_span(kData, kDataSize));
  // auto buffer = StreamParser::FromExternalMemory(std::move(external_memory));

  return StreamParserBuffer::CopyFrom(data, data_size,
                                       side_data, side_data_size,
                                               is_key_frame,
                                               type,
                                               track_id);

}

void StreamParserBufferPool::PoolImpl::Shutdown() {
  base::AutoLock auto_lock(lock_);
  is_shutdown_ = true;
  buffers_.clear();
}

StreamParserBufferPool::StreamParserBufferPool() : pool_(new PoolImpl()) {}

StreamParserBufferPool::~StreamParserBufferPool() {
  pool_->Shutdown();
}

scoped_refptr<StreamParserBuffer> StreamParserBufferPool::CopyFrom(const uint8_t* data,
                                               size_t data_size,
                                               bool is_key_frame,
                                               Type type,
                                               TrackId track_id) {
  return pool_->CopyFrom(data, data_size, is_key_frame, type, track_id);
}
//  scoped_refptr<StreamParserBuffer> CopyFrom(const uint8_t* data,
//                                             int data_size,
//                                             const uint8_t* side_data,
//                                             int side_data_size,
//                                             bool is_key_frame,
//                                             Type type,
//                                             TrackId track_id);

scoped_refptr<StreamParserBuffer> StreamParserBufferPool::CopyFrom(const uint8_t* data,
                                               size_t data_size,
                                               const uint8_t* side_data,
                                               size_t side_data_size,
                                               bool is_key_frame,
                                               Type type,
                                               TrackId track_id) {
  return pool_->CopyFrom(data, data_size, side_data, side_data_size, is_key_frame, type, track_id);
}
size_t StreamParserBufferPool::GetPoolSizeForTesting() const {
  return pool_->get_pool_size_for_testing();
}

//  void StreamParserBufferPool::SetTickClockForTesting(const base::TickClock* tick_clock) {
//    pool_->set_tick_clock_for_testing(tick_clock);
//  }

}  // namespace media
