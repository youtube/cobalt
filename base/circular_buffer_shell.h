// Copyright (c) 2014 Google Inc. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_CIRCULAR_BUFFER_SHELL_H_
#define BASE_CIRCULAR_BUFFER_SHELL_H_

#include "base/base_export.h"
#include "base/synchronization/lock.h"

namespace base {

// A thread-safe circular buffer implementation.
class BASE_EXPORT CircularBufferShell {
 public:
  explicit CircularBufferShell(size_t max_capacity);
  ~CircularBufferShell();

  // Clears out all data in the buffer, releasing any allocated memory.
  // Idempotent.
  void Clear();

  // Reads the requested amount of data into the given buffer, writing the
  // number of bytes actually read into the bytes_read paramter. If there is
  // less data then requested, then the remaining data will be consumed.
  void Read(void *destination, size_t length, size_t *bytes_read);

  // Writes the given data into the circular buffer. Returns false if the buffer
  // could not be expanded to hold the new data. If returning false,
  // bytes_written will not be set, and the buffer will remain unchanged.
  bool Write(const void *source, size_t length, size_t *bytes_written);

  // Returns the length of the data left in the buffer to read.
  size_t GetLength() const;

 private:
  // Ensures that there is enough capacity to write length bytes to the
  // buffer. Returns false if it was unable to ensure that capacity due to an
  // allocation error, or if it would surpass the configured maximum capacity.
  bool EnsureCapacityToWrite(size_t length);

  // Increases the capacity to the given absolute size in bytes. Returns false
  // if there was an allocation error, or it would surpass the configured
  // maximum capacity.
  bool IncreaseCapacityTo(size_t capacity);

  // Private workhorse for Read without the parameter validation or locking.
  void ReadUnchecked(void *destination, size_t length, size_t *bytes_read);

  // Gets a pointer to the current read position.
  const void *GetReadPointer() const;

  // Gets a pointer to the current write position.
  void *GetWritePointer() const;

  // Gets the current write position.
  size_t GetWritePosition() const;

  const size_t max_capacity_;
  void *buffer_;
  size_t capacity_;
  size_t length_;
  size_t read_position_;
  mutable base::Lock lock_;
};

}  // namespace base

#endif  // BASE_CIRCULAR_BUFFER_SHELL_H_
