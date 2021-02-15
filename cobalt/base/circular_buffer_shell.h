// Copyright (c) 2014 Google Inc. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_CIRCULAR_BUFFER_SHELL_H_
#define BASE_CIRCULAR_BUFFER_SHELL_H_

#include "base/base_export.h"
#include "base/synchronization/lock.h"

namespace base {

// A thread-safe circular buffer implementation.
// TODO: Returns the size in Read(), Peek(), and Skip() as a return
// value.
class BASE_EXPORT CircularBufferShell {
 public:
  enum ReserveType { kDoNotReserve, kReserve };

  explicit CircularBufferShell(size_t max_capacity,
                               ReserveType reserve_type = kDoNotReserve);
  ~CircularBufferShell();

  // Clears out all data in the buffer, releasing any allocated memory.
  // Idempotent.
  void Clear();

  // Reads the requested amount of data into the given buffer, writing the
  // number of bytes actually read into the bytes_read parameter. If there is
  // less data then requested, then the remaining data will be consumed.
  void Read(void* destination, size_t length, size_t* bytes_read);

  // It works the same as Read() except:
  // 1. It doesn't modify the buffer in any way.
  // 2. It allows the caller to specify an offset inside the buffer where the
  // peek starts.
  void Peek(void* destination, size_t length, size_t source_offset,
            size_t* bytes_peeked) const;

  // Advance the buffer cursor without reading any data.
  void Skip(size_t length, size_t* bytes_skipped);

  // Writes the given data into the circular buffer. Returns false if the buffer
  // could not be expanded to hold the new data. If returning false,
  // bytes_written will not be set, and the buffer will remain unchanged.
  // TODO: Remove bytes_written.  Because Write returns false when
  // the buffer cannot hold all data, bytes_written isn't useful here unless we
  // allow partial write.
  bool Write(const void* source, size_t length, size_t* bytes_written);

  // Returns the length of the data left in the buffer to read.
  size_t GetLength() const;

  // Returns the maximum capacity this circular buffer can grow to.
  size_t GetMaxCapacity() const;

  // Increase the max capacity to |new_max_capacity| which has to be greater
  // than the previous one.  The content of the class will be kept.
  void IncreaseMaxCapacityTo(size_t new_max_capacity);

 private:
  // Ensures that there is enough capacity to write length bytes to the
  // buffer. Returns false if it was unable to ensure that capacity due to an
  // allocation error, or if it would surpass the configured maximum capacity.
  bool EnsureCapacityToWrite_Locked(size_t length);

  // Increases the capacity to the given absolute size in bytes. Returns false
  // if there was an allocation error, or it would surpass the configured
  // maximum capacity.
  bool IncreaseCapacityTo_Locked(size_t capacity);

  // Private workhorse for Read without the parameter validation or locking.
  // When |destination| is NULL, it purely calculates the the bytes that would
  // have been read.
  // Note that the function doesn't advance the read cursor or modify the
  // length.  It is caller's responsibility to adjust |read_position_| and
  // |length_| according to the return value, which is the actual number of
  // bytes read.
  void ReadUnchecked_Locked(void* destination, size_t destination_length,
                            size_t source_offset, size_t* bytes_read) const;

  // The same the as above functions except that it also advance the
  // |read_position_| and adjust the |length_| accordingly.
  void ReadAndAdvanceUnchecked_Locked(void* destination,
                                      size_t destination_length,
                                      size_t* bytes_read);

  // Gets a pointer to the current write position.
  void* GetWritePointer_Locked() const;

  // Gets the current write position.
  size_t GetWritePosition_Locked() const;

  size_t max_capacity_;
  void* buffer_;
  size_t capacity_;
  size_t length_;
  size_t read_position_;
  mutable base::Lock lock_;
};

}  // namespace base

#endif  // BASE_CIRCULAR_BUFFER_SHELL_H_
