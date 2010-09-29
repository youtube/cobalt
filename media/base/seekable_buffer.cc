// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/seekable_buffer.h"

#include <algorithm>

#include "base/logging.h"
#include "media/base/data_buffer.h"

namespace media {

SeekableBuffer::SeekableBuffer(size_t backward_capacity,
                               size_t forward_capacity)
    : current_buffer_offset_(0),
      backward_capacity_(backward_capacity),
      backward_bytes_(0),
      forward_capacity_(forward_capacity),
      forward_bytes_(0),
      current_time_(StreamSample::kInvalidTimestamp) {
  current_buffer_ = buffers_.begin();
}

SeekableBuffer::~SeekableBuffer() {
}

void SeekableBuffer::Clear() {
  buffers_.clear();
  current_buffer_ = buffers_.begin();
  current_buffer_offset_ = 0;
  backward_bytes_ = 0;
  forward_bytes_ = 0;
  current_time_ = StreamSample::kInvalidTimestamp;
}


size_t SeekableBuffer::Read(uint8* data, size_t size) {
  DCHECK(data);
  return InternalRead(data, size, true);
}

size_t SeekableBuffer::Peek(uint8* data, size_t size) {
  DCHECK(data);
  return InternalRead(data, size, false);
}

bool SeekableBuffer::GetCurrentChunk(const uint8** data, size_t* size) const {
  BufferQueue::iterator current_buffer = current_buffer_;
  size_t current_buffer_offset = current_buffer_offset_;
  // Advance position if we are in the end of the current buffer.
  while (current_buffer != buffers_.end() &&
         current_buffer_offset >= (*current_buffer)->GetDataSize()) {
    ++current_buffer;
    current_buffer_offset = 0;
  }
  if (current_buffer == buffers_.end())
    return false;
  *data = (*current_buffer)->GetData() + current_buffer_offset;
  *size = (*current_buffer)->GetDataSize() - current_buffer_offset;
  return true;
}

bool SeekableBuffer::Append(Buffer* buffer_in) {
  if (buffers_.empty() && buffer_in->GetTimestamp().InMicroseconds() > 0) {
    current_time_ = buffer_in->GetTimestamp();
  }

  // Since the forward capacity is only used to check the criteria for buffer
  // full, we always append data to the buffer.
  buffers_.push_back(scoped_refptr<Buffer>(buffer_in));

  // After we have written the first buffer, update |current_buffer_| to point
  // to it.
  if (current_buffer_ == buffers_.end()) {
    DCHECK_EQ(0u, forward_bytes_);
    current_buffer_ = buffers_.begin();
  }

  // Update the |forward_bytes_| counter since we have more bytes.
  forward_bytes_ += buffer_in->GetDataSize();

  // Advise the user to stop append if the amount of forward bytes exceeds
  // the forward capacity. A false return value means the user should stop
  // appending more data to this buffer.
  if (forward_bytes_ >= forward_capacity_)
    return false;
  return true;
}

bool SeekableBuffer::Append(const uint8* data, size_t size) {
  if (size > 0) {
    DataBuffer* data_buffer = new DataBuffer(size);
    memcpy(data_buffer->GetWritableData(), data, size);
    data_buffer->SetDataSize(size);
    return Append(data_buffer);
  } else {
    // Return true if we have forward capacity.
    return forward_bytes_ < forward_capacity_;
  }
}

bool SeekableBuffer::Seek(int32 offset) {
  if (offset > 0)
    return SeekForward(offset);
  else if (offset < 0)
    return SeekBackward(-offset);
  return true;
}

bool SeekableBuffer::SeekForward(size_t size) {
  // Perform seeking forward only if we have enough bytes in the queue.
  if (size > forward_bytes_)
    return false;

  // Do a read of |size| bytes.
  size_t taken = InternalRead(NULL, size, true);
  DCHECK_EQ(taken, size);
  return true;
}

bool SeekableBuffer::SeekBackward(size_t size) {
  if (size > backward_bytes_)
    return false;
  // Record the number of bytes taken.
  size_t taken = 0;
  // Loop until we taken enough bytes and rewind by the desired |size|.
  while (taken < size) {
    // |current_buffer_| can never be invalid when we are in this loop. It can
    // only be invalid before any data is appended. The invalid case should be
    // handled by checks before we enter this loop.
    DCHECK(current_buffer_ != buffers_.end());

    // We try to consume at most |size| bytes in the backward direction. We also
    // have to account for the offset we are in the current buffer, so take the
    // minimum between the two to determine the amount of bytes to take from the
    // current buffer.
    size_t consumed = std::min(size - taken, current_buffer_offset_);

    // Decreases the offset in the current buffer since we are rewinding.
    current_buffer_offset_ -= consumed;

    // Increase the amount of bytes taken in the backward direction. This
    // determines when to stop the loop.
    taken += consumed;

    // Forward bytes increases and backward bytes decreases by the amount
    // consumed in the current buffer.
    forward_bytes_ += consumed;
    backward_bytes_ -= consumed;
    DCHECK_GE(backward_bytes_, 0u);

    // The current buffer pointed by current iterator has been consumed. Move
    // the iterator backward so it points to the previous buffer.
    if (current_buffer_offset_ == 0) {
      if (current_buffer_ == buffers_.begin())
        break;
      // Move the iterator backward.
      --current_buffer_;
      // Set the offset into the current buffer to be the buffer size as we
      // are preparing for rewind for next iteration.
      current_buffer_offset_ = (*current_buffer_)->GetDataSize();
    }
  }

  UpdateCurrentTime(current_buffer_, current_buffer_offset_);

  DCHECK_EQ(taken, size);
  return true;
}

void SeekableBuffer::EvictBackwardBuffers() {
  // Advances the iterator until we hit the current pointer.
  while (backward_bytes_ > backward_capacity_) {
    BufferQueue::iterator i = buffers_.begin();
    if (i == current_buffer_)
      break;
    scoped_refptr<Buffer> buffer = *i;
    backward_bytes_ -= buffer->GetDataSize();
    DCHECK_GE(backward_bytes_, 0u);

    buffers_.erase(i);
  }
}

size_t SeekableBuffer::InternalRead(uint8* data, size_t size,
                                    bool advance_position) {
  // Counts how many bytes are actually read from the buffer queue.
  size_t taken = 0;

  BufferQueue::iterator current_buffer = current_buffer_;
  size_t current_buffer_offset = current_buffer_offset_;

  while (taken < size) {
    // |current_buffer| is valid since the first time this buffer is appended
    // with data.
    if (current_buffer == buffers_.end())
      break;

    scoped_refptr<Buffer> buffer = *current_buffer;

    // Find the right amount to copy from the current buffer referenced by
    // |buffer|. We shall copy no more than |size| bytes in total and each
    // single step copied no more than the current buffer size.
    size_t copied = std::min(size - taken,
                             buffer->GetDataSize() - current_buffer_offset);

    // |data| is NULL if we are seeking forward, so there's no need to copy.
    if (data)
      memcpy(data + taken, buffer->GetData() + current_buffer_offset, copied);

    // Increase total number of bytes copied, which regulates when to end this
    // loop.
    taken += copied;

    // We have read |copied| bytes from the current buffer. Advances the offset.
    current_buffer_offset += copied;

    // The buffer has been consumed.
    if (current_buffer_offset == buffer->GetDataSize()) {
      if (advance_position) {
        // Next buffer may not have timestamp, so we need to update current
        // timestamp before switching to the next buffer.
        UpdateCurrentTime(current_buffer, current_buffer_offset);
      }

      BufferQueue::iterator next = current_buffer;
      ++next;
      // If we are at the last buffer, don't advance.
      if (next == buffers_.end())
        break;

      // Advances the iterator.
      current_buffer = next;
      current_buffer_offset = 0;
    }
  }

  if (advance_position) {
    // We have less forward bytes and more backward bytes. Updates these
    // counters by |taken|.
    forward_bytes_ -= taken;
    backward_bytes_ += taken;
    DCHECK_GE(forward_bytes_, 0u);
    DCHECK(current_buffer_ != buffers_.end() || forward_bytes_ == 0u);

    current_buffer_ = current_buffer;
    current_buffer_offset_ = current_buffer_offset;

    UpdateCurrentTime(current_buffer_, current_buffer_offset_);
    EvictBackwardBuffers();
  }

  return taken;
}

void SeekableBuffer::UpdateCurrentTime(BufferQueue::iterator buffer,
                                       size_t offset) {
  // Garbage values are unavoidable, so this check will remain.
  if (buffer != buffers_.end() &&
      (*buffer)->GetTimestamp().InMicroseconds() > 0) {
    int64 time_offset = ((*buffer)->GetDuration().InMicroseconds() *
                         offset) / (*buffer)->GetDataSize();

    current_time_ = (*buffer)->GetTimestamp() +
        base::TimeDelta::FromMicroseconds(time_offset);
  }
}

}  // namespace media
