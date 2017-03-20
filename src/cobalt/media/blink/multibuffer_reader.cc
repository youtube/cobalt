// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include <algorithm>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/location.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "cobalt/media/blink/multibuffer_reader.h"
#include "net/base/net_errors.h"

namespace media {

MultiBufferReader::MultiBufferReader(
    MultiBuffer* multibuffer, int64_t start, int64_t end,
    const base::Callback<void(int64_t, int64_t)>& progress_callback)
    : multibuffer_(multibuffer),
      // If end is -1, we use a very large (but still supported) value instead.
      end_(end == -1LL ? (1LL << (multibuffer->block_size_shift() + 30)) : end),
      preload_high_(0),
      preload_low_(0),
      max_buffer_forward_(0),
      max_buffer_backward_(0),
      current_buffer_size_(0),
      pinned_range_(0, 0),
      pos_(start),
      preload_pos_(-1),
      loading_(true),
      current_wait_size_(0),
      progress_callback_(progress_callback),
      weak_factory_(this) {
  DCHECK_GE(start, 0);
  DCHECK_GE(end_, 0);
}

MultiBufferReader::~MultiBufferReader() {
  PinRange(0, 0);
  multibuffer_->RemoveReader(preload_pos_, this);
  multibuffer_->IncrementMaxSize(-current_buffer_size_);
  multibuffer_->CleanupWriters(preload_pos_);
}

void MultiBufferReader::Seek(int64_t pos) {
  DCHECK_GE(pos, 0);
  if (pos == pos_) return;
  PinRange(block(pos - max_buffer_backward_),
           block_ceil(pos + max_buffer_forward_));

  multibuffer_->RemoveReader(preload_pos_, this);
  MultiBufferBlockId old_preload_pos = preload_pos_;
  preload_pos_ = block(pos);
  pos_ = pos;
  UpdateInternalState();
  multibuffer_->CleanupWriters(old_preload_pos);
}

void MultiBufferReader::SetMaxBuffer(int64_t buffer_size) {
  // Safe, because we know this doesn't actually prune the cache right away.
  int64_t new_buffer_size = block_ceil(buffer_size);
  multibuffer_->IncrementMaxSize(new_buffer_size - current_buffer_size_);
  current_buffer_size_ = new_buffer_size;
}

void MultiBufferReader::SetPinRange(int64_t backward, int64_t forward) {
  // Safe, because we know this doesn't actually prune the cache right away.
  max_buffer_backward_ = backward;
  max_buffer_forward_ = forward;
  PinRange(block(pos_ - max_buffer_backward_),
           block_ceil(pos_ + max_buffer_forward_));
}

int64_t MultiBufferReader::Available() const {
  int64_t unavailable_byte_pos =
      static_cast<int64_t>(multibuffer_->FindNextUnavailable(block(pos_)))
      << multibuffer_->block_size_shift();
  return std::max<int64_t>(0, unavailable_byte_pos - pos_);
}

int64_t MultiBufferReader::TryRead(uint8_t* data, int64_t len) {
  DCHECK_GT(len, 0);
  current_wait_size_ = 0;
  cb_.Reset();
  DCHECK_LE(pos_ + len, end_);
  const MultiBuffer::DataMap& data_map = multibuffer_->map();
  MultiBuffer::DataMap::const_iterator i = data_map.find(block(pos_));
  int64_t p = pos_;
  int64_t bytes_read = 0;
  while (bytes_read < len) {
    if (i == data_map.end()) break;
    if (i->first != block(p)) break;
    if (i->second->end_of_stream()) break;
    size_t offset = p & ((1LL << multibuffer_->block_size_shift()) - 1);
    if (offset > static_cast<size_t>(i->second->data_size())) break;
    size_t tocopy =
        std::min<size_t>(len - bytes_read, i->second->data_size() - offset);
    memcpy(data, i->second->data() + offset, tocopy);
    data += tocopy;
    bytes_read += tocopy;
    p += tocopy;
    ++i;
  }
  Seek(p);
  return bytes_read;
}

int MultiBufferReader::Wait(int64_t len, const base::Closure& cb) {
  DCHECK_LE(pos_ + len, end_);
  DCHECK_NE(Available(), -1);
  DCHECK_LE(len, max_buffer_forward_);
  current_wait_size_ = len;

  cb_.Reset();
  UpdateInternalState();

  if (Available() >= current_wait_size_) {
    return net::OK;
  } else {
    cb_ = cb;
    return net::ERR_IO_PENDING;
  }
}

void MultiBufferReader::SetPreload(int64_t preload_high, int64_t preload_low) {
  DCHECK_GE(preload_high, preload_low);
  multibuffer_->RemoveReader(preload_pos_, this);
  preload_pos_ = block(pos_);
  preload_high_ = preload_high;
  preload_low_ = preload_low;
  UpdateInternalState();
}

bool MultiBufferReader::IsLoading() const { return loading_; }

void MultiBufferReader::CheckWait() {
  if (!cb_.is_null() &&
      (Available() >= current_wait_size_ || Available() == -1)) {
    // We redirect the call through a weak pointer to ourselves to guarantee
    // there are no callbacks from us after we've been destroyed.
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::Bind(&MultiBufferReader::Call, weak_factory_.GetWeakPtr(),
                   base::ResetAndReturn(&cb_)));
  }
}

void MultiBufferReader::Call(const base::Closure& cb) const { cb.Run(); }

void MultiBufferReader::UpdateEnd(MultiBufferBlockId p) {
  auto i = multibuffer_->map().find(p - 1);
  if (i != multibuffer_->map().end() && i->second->end_of_stream()) {
    // This is an upper limit because the last-to-one block is allowed
    // to be smaller than the rest of the blocks.
    int64_t size_upper_limit = static_cast<int64_t>(p)
                               << multibuffer_->block_size_shift();
    end_ = std::min(end_, size_upper_limit);
  }
}

void MultiBufferReader::NotifyAvailableRange(
    const Interval<MultiBufferBlockId>& range) {
  // Update end_ if we can.
  if (range.end > range.begin) {
    UpdateEnd(range.end);
  }
  UpdateInternalState();
  if (!progress_callback_.is_null()) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::Bind(progress_callback_, static_cast<int64_t>(range.begin)
                                           << multibuffer_->block_size_shift(),
                   (static_cast<int64_t>(range.end)
                    << multibuffer_->block_size_shift()) +
                       multibuffer_->UncommittedBytesAt(range.end)));
  }
}

void MultiBufferReader::UpdateInternalState() {
  int64_t effective_preload = loading_ ? preload_high_ : preload_low_;

  loading_ = false;
  if (preload_pos_ == -1) {
    preload_pos_ = block(pos_);
    DCHECK_GE(preload_pos_, 0);
  }

  // Note that we might not have been added to the multibuffer,
  // removing ourselves is a no-op in that case.
  multibuffer_->RemoveReader(preload_pos_, this);

  // We explicitly allow preloading to go beyond the pinned region in the cache.
  // It only happens when we want to preload something into the disk cache.
  // Thus it is possible to have blocks between our current reading position
  // and preload_pos_ be unavailable. When we get a Seek() call (possibly
  // through TryRead()) we reset the preload_pos_ to the current reading
  // position, and preload_pos_ will become the first unavailable block after
  // our current reading position again.
  preload_pos_ = multibuffer_->FindNextUnavailable(preload_pos_);
  UpdateEnd(preload_pos_);
  DCHECK_GE(preload_pos_, 0);

  MultiBuffer::BlockId max_preload = block_ceil(
      std::min(end_, pos_ + std::max(effective_preload, current_wait_size_)));

  DVLOG(3) << "UpdateInternalState"
           << " pp = " << preload_pos_
           << " block_ceil(end_) = " << block_ceil(end_) << " end_ = " << end_
           << " max_preload " << max_preload;

  if (preload_pos_ < block_ceil(end_)) {
    if (preload_pos_ < max_preload) {
      loading_ = true;
      multibuffer_->AddReader(preload_pos_, this);
    } else if (multibuffer_->Contains(preload_pos_ - 1)) {
      --preload_pos_;
      multibuffer_->AddReader(preload_pos_, this);
    }
  }
  CheckWait();
}

void MultiBufferReader::PinRange(MultiBuffer::BlockId begin,
                                 MultiBuffer::BlockId end) {
  // Use a rangemap to compute the diff in pinning.
  IntervalMap<MultiBuffer::BlockId, int32_t> tmp;
  tmp.IncrementInterval(pinned_range_.begin, pinned_range_.end, -1);
  tmp.IncrementInterval(begin, end, 1);
  multibuffer_->PinRanges(tmp);
  pinned_range_.begin = begin;
  pinned_range_.end = end;
}

}  // namespace media
