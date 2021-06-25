// Copyright (c) 2014 Google Inc. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cobalt/base/circular_buffer_shell.h"

#include <stdint.h>

#include <algorithm>

#include "base/logging.h"
#include "build/build_config.h"

#if defined(STARBOARD)
#include "starboard/memory.h"
#define malloc SbMemoryAllocate
#define realloc SbMemoryReallocate
#define free SbMemoryDeallocate
#endif

static inline void* add_to_pointer(void* pointer, size_t amount) {
  return static_cast<uint8_t*>(pointer) + amount;
}

static inline const void* add_to_pointer(const void* pointer, size_t amount) {
  return static_cast<const uint8_t*>(pointer) + amount;
}

namespace base {

CircularBufferShell::CircularBufferShell(
    size_t max_capacity, ReserveType reserve_type /*= kDoNotReserve*/)
    : max_capacity_(max_capacity),
      buffer_(NULL),
      capacity_(0),
      length_(0),
      read_position_(0) {
  if (reserve_type == kReserve) {
    base::AutoLock l(lock_);
    IncreaseCapacityTo_Locked(max_capacity_);
  }
}

CircularBufferShell::~CircularBufferShell() { Clear(); }

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

void CircularBufferShell::Read(void* destination, size_t length,
                               size_t* bytes_read) {
  base::AutoLock l(lock_);
  DCHECK(destination != NULL || length == 0);
  if (destination == NULL) length = 0;

  ReadAndAdvanceUnchecked_Locked(destination, length, bytes_read);
}

void CircularBufferShell::Peek(void* destination, size_t length,
                               size_t source_offset,
                               size_t* bytes_peeked) const {
  base::AutoLock l(lock_);
  DCHECK(destination != NULL || length == 0);
  if (destination == NULL) length = 0;

  ReadUnchecked_Locked(destination, length, source_offset, bytes_peeked);
}

void CircularBufferShell::Skip(size_t length, size_t* bytes_skipped) {
  base::AutoLock l(lock_);
  ReadAndAdvanceUnchecked_Locked(NULL, length, bytes_skipped);
}

bool CircularBufferShell::Write(const void* source, size_t length,
                                size_t* bytes_written) {
  base::AutoLock l(lock_);
  DCHECK(source != NULL || length == 0);
  if (source == NULL) length = 0;

  if (!EnsureCapacityToWrite_Locked(length)) {
    return false;
  }

  size_t produced = 0;
  while (true) {
    size_t remaining = length - produced;

    // In this pass, write up to the contiguous space left.
    size_t to_write =
        std::min(remaining, capacity_ - GetWritePosition_Locked());
    if (to_write == 0) break;

    // Copy this segment and do the accounting.
    void* destination = GetWritePointer_Locked();
    const void* src = add_to_pointer(source, produced);
    memcpy(destination, src, to_write);
    length_ += to_write;
    produced += to_write;
  }

  if (bytes_written) *bytes_written = produced;
  return true;
}

size_t CircularBufferShell::GetLength() const {
  base::AutoLock l(lock_);
  return length_;
}

void CircularBufferShell::ReadUnchecked_Locked(void* destination,
                                               size_t destination_length,
                                               size_t source_offset,
                                               size_t* bytes_read) const {
  DCHECK(destination != NULL || bytes_read != NULL);

  lock_.AssertAcquired();

  size_t dummy = 0;
  if (!bytes_read) {
    bytes_read = &dummy;
  }

  // Return immediately if the CircularBuffer is empty or if |source_offset| is
  // greater or equal than |length_|.
  if (capacity_ == 0 || source_offset >= length_) {
    *bytes_read = 0;
    return;
  }

  size_t consumed = 0;
  size_t source_length = length_ - source_offset;
  size_t read_position = (read_position_ + source_offset) % capacity_;

  while (true) {
    size_t remaining = std::min(source_length, destination_length - consumed);

    // In this pass, read the remaining data that is contiguous.
    size_t to_read = std::min(remaining, capacity_ - read_position);
    if (to_read == 0) break;

    // Copy this segment and do the accounting.
    const void* source = add_to_pointer(buffer_, read_position);
    if (destination) {
      void* dest = add_to_pointer(destination, consumed);
      memcpy(dest, source, to_read);
    }
    source_length -= to_read;
    read_position = (read_position + to_read) % capacity_;
    consumed += to_read;
  }

  *bytes_read = consumed;
}

void CircularBufferShell::ReadAndAdvanceUnchecked_Locked(
    void* destination, size_t destination_length, size_t* bytes_read) {
  lock_.AssertAcquired();

  size_t dummy = 0;
  if (!bytes_read) {
    bytes_read = &dummy;
  }

  // Return immediately if the CircularBuffer is empty.
  if (capacity_ == 0) {
    *bytes_read = 0;
    return;
  }

  ReadUnchecked_Locked(destination, destination_length, 0, bytes_read);
  length_ -= *bytes_read;
  read_position_ = (read_position_ + *bytes_read) % capacity_;
}

void* CircularBufferShell::GetWritePointer_Locked() const {
  lock_.AssertAcquired();

  return add_to_pointer(buffer_, GetWritePosition_Locked());
}

size_t CircularBufferShell::GetWritePosition_Locked() const {
  lock_.AssertAcquired();

  return (read_position_ + length_) % capacity_;
}

bool CircularBufferShell::EnsureCapacityToWrite_Locked(size_t length) {
  lock_.AssertAcquired();

  if (capacity_ - length_ < length) {
    size_t capacity = std::max(2 * capacity_, length_ + length);
    if (capacity > max_capacity_) capacity = max_capacity_;

    // New capacity still won't be enough.
    if (capacity - length_ < length) {
      return false;
    }

    return IncreaseCapacityTo_Locked(capacity);
  }

  return true;
}

bool CircularBufferShell::IncreaseCapacityTo_Locked(size_t capacity) {
  lock_.AssertAcquired();

  if (capacity <= capacity_) {
    return true;
  }

  // If the data isn't wrapped, we can just use realloc.
  if (buffer_ != NULL && read_position_ + length_ <= capacity_) {
    void* result = realloc(buffer_, capacity);
    if (result == NULL) {
      return false;
    }
    capacity_ = capacity;
    buffer_ = result;
    return true;
  }

  void* buffer = malloc(capacity);
  if (buffer == NULL) {
    return false;
  }

  // Read does everything we want, but it will trounce length_.
  size_t length = length_;

  // Copy the data over to the new buffer.
  ReadUnchecked_Locked(buffer, length_, 0, NULL);

  // Adjust the accounting.
  length_ = length;
  read_position_ = 0;
  capacity_ = capacity;
  free(buffer_);
  buffer_ = buffer;
  return true;
}

size_t CircularBufferShell::GetMaxCapacity() const {
  base::AutoLock l(lock_);

  return max_capacity_;
}

void CircularBufferShell::IncreaseMaxCapacityTo(size_t new_max_capacity) {
  base::AutoLock l(lock_);

  DCHECK_GT(new_max_capacity, max_capacity_);
  if (new_max_capacity > max_capacity_) {
    max_capacity_ = new_max_capacity;
  }
}

}  // namespace base
