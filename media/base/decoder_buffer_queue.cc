// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/decoder_buffer_queue.h"

#include "base/logging.h"
#include "media/base/decoder_buffer.h"
#include "media/base/timestamp_constants.h"

namespace media {

DecoderBufferQueue::DecoderBufferQueue()
    : earliest_valid_timestamp_(kNoTimestamp), data_size_(0) {}

DecoderBufferQueue::~DecoderBufferQueue() = default;

// TODO(dalecurtis): This whole class can be simplified significantly; there's
// no reason to track an in order queue, instead track min + max timestamps.
void DecoderBufferQueue::Push(scoped_refptr<DecoderBuffer> buffer) {
  DCHECK(!buffer->end_of_stream());

  queue_.push_back(buffer);
  data_size_ += buffer->data_size();

  // TODO(scherkus): FFmpeg returns some packets with no timestamp after
  // seeking. Fix and turn this into CHECK(). See http://crbug.com/162192
  if (buffer->timestamp() == kNoTimestamp) {
    DVLOG(1) << "Buffer has no timestamp";
    return;
  }

  if (earliest_valid_timestamp_ == kNoTimestamp) {
    earliest_valid_timestamp_ = buffer->timestamp();
  }

  if (buffer->timestamp() < earliest_valid_timestamp_) {
    DVLOG(2) << "Out of order timestamps: "
             << buffer->timestamp().InMicroseconds() << " vs. "
             << earliest_valid_timestamp_.InMicroseconds();
    return;
  }

  earliest_valid_timestamp_ = buffer->timestamp();
  in_order_queue_.emplace_back(std::move(buffer));
}

scoped_refptr<DecoderBuffer> DecoderBufferQueue::Pop() {
  scoped_refptr<DecoderBuffer> buffer = std::move(queue_.front());
  queue_.pop_front();

  size_t buffer_data_size = buffer->data_size();
  DCHECK_LE(buffer_data_size, data_size_);
  data_size_ -= buffer_data_size;

  if (!in_order_queue_.empty() && in_order_queue_.front() == buffer)
    in_order_queue_.pop_front();

  return buffer;
}

void DecoderBufferQueue::Clear() {
  queue_.clear();
  data_size_ = 0;
  in_order_queue_.clear();
  earliest_valid_timestamp_ = kNoTimestamp;
}

bool DecoderBufferQueue::IsEmpty() {
  return queue_.empty();
}

base::TimeDelta DecoderBufferQueue::Duration() {
  if (in_order_queue_.size() < 2)
    return base::TimeDelta();

  base::TimeDelta start = in_order_queue_.front()->timestamp();
  base::TimeDelta end = in_order_queue_.back()->timestamp();
  return end - start;
}

}  // namespace media
