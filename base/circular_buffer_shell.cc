// Copyright (c) 2014 Google Inc. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/circular_buffer_shell.h"

#include <stdint.h>

#include <algorithm>

#include "base/logging.h"

static inline void *add_to_pointer(void *pointer, size_t amount) {
    return static_cast<uint8_t *>(pointer) + amount;
}

static inline const void *add_to_pointer(const void *pointer, size_t amount) {
    return static_cast<const uint8_t *>(pointer) + amount;
}

namespace base {

CircularBufferShell::CircularBufferShell(size_t max_capacity)
    : max_capacity_(max_capacity),
      buffer_(NULL),
      capacity_(0),
      length_(0),
      read_position_(0) {
}

CircularBufferShell::~CircularBufferShell() {
  Clear();
}

void CircularBufferShell::Clear() {
  base::AutoLock l(lock_);
  if (buffer_ != NULL) {
    free(buffer_);
    buffer_ = NULL;
  }

  capacity_ = 0;
  length_ = 0;
  read_position_ = 0;
}

void CircularBufferShell::Read(void *destination, size_t length,
                               size_t *bytes_read) {
  base::AutoLock l(lock_);
  DCHECK(destination != NULL || length == 0);
  if (destination == NULL)
    length = 0;

  ReadUnchecked(destination, length, bytes_read);
}

bool CircularBufferShell::Write(const void *source, size_t length,
                                size_t *bytes_written) {
  base::AutoLock l(lock_);
  DCHECK(source != NULL || length == 0);
  if (source == NULL)
    length = 0;

  if (!EnsureCapacityToWrite(length)) {
    return false;
  }

  size_t produced = 0;
  while (true) {
    size_t remaining = length - produced;

    // In this pass, write up to the contiguous space left.
    size_t to_write = std::min(remaining, capacity_ - GetWritePosition());
    if (to_write == 0)
      break;

    // Copy this segment and do the accounting.
    void *destination = GetWritePointer();
    const void *src = add_to_pointer(source, produced);
    memcpy(destination, src, to_write);
    length_ += to_write;
    produced += to_write;
  }

  if (bytes_written)
    *bytes_written = produced;
  return true;
}

size_t CircularBufferShell::GetLength() const {
  base::AutoLock l(lock_);
  return length_;
}


void CircularBufferShell::ReadUnchecked(void *destination, size_t length,
                                        size_t *bytes_read) {
  size_t consumed = 0;
  while (true) {
    size_t remaining = std::min(length_, length - consumed);

    // In this pass, read the remaining data that is contiguous.
    size_t to_read = std::min(remaining, capacity_ - read_position_);
    if (to_read == 0)
      break;

    // Copy this segment and do the accounting.
    const void *source = GetReadPointer();
    void *dest = add_to_pointer(destination, consumed);
    memcpy(dest, source, to_read);
    length_ -= to_read;
    read_position_ = (read_position_ + to_read) % capacity_;
    consumed += to_read;
  }

  if (bytes_read)
    *bytes_read = consumed;
}

const void *CircularBufferShell::GetReadPointer() const {
  return add_to_pointer(buffer_, read_position_);
}

void *CircularBufferShell::GetWritePointer() const {
  return add_to_pointer(buffer_, GetWritePosition());
}

size_t CircularBufferShell::GetWritePosition() const {
  return (read_position_ + length_) % capacity_;
}

bool CircularBufferShell::EnsureCapacityToWrite(size_t length) {
  if (capacity_ - length_ < length) {
    size_t capacity = std::max(2 * capacity_, length_ + length);
    if (capacity > max_capacity_)
      capacity = max_capacity_;

    // New capacity still won't be enough.
    if (capacity - length_ < length) {
      return false;
    }

    return IncreaseCapacityTo(capacity);
  }

  return true;
}

bool CircularBufferShell::IncreaseCapacityTo(size_t capacity) {
  if (capacity <= capacity_) {
    return true;
  }

  // If the data isn't wrapped, we can just use realloc.
  if (buffer_ != NULL && read_position_ + length_ <= capacity_) {
    void *result = realloc(buffer_, capacity);
    if (result == NULL) {
      return false;
    }
    capacity_ = capacity;
    buffer_ = result;
    return true;
  }

  void *buffer = malloc(capacity);
  if (buffer == NULL) {
    return false;
  }

  // Read does everything we want, but it will trounce length_.
  size_t length = length_;

  // Copy the data over to the new buffer.
  ReadUnchecked(buffer, length_, NULL);

  // Adjust the accounting.
  length_ = length;
  read_position_ = 0;
  capacity_ = capacity;
  free(buffer_);
  buffer_ = buffer;
  return true;
}

}  // namespace base
