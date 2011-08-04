// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// A simple implementation of Buffer that takes ownership of the given data
// pointer.
//
// DataBuffer assumes that memory was allocated with new uint8[].

#ifndef MEDIA_BASE_DATA_BUFFER_H_
#define MEDIA_BASE_DATA_BUFFER_H_

#include "base/memory/scoped_ptr.h"
#include "media/base/buffers.h"

namespace media {

class MEDIA_EXPORT DataBuffer : public Buffer {
 public:
  // Takes ownership of the passed |buffer|, assumes valid data of size
  // |buffer_size|.
  DataBuffer(uint8* buffer, size_t buffer_size);

  // Allocates buffer of size |buffer_size|. If |buffer_size| is 0, |data_| is
  // set to a NULL ptr.
  explicit DataBuffer(size_t buffer_size);

  // Buffer implementation.
  virtual const uint8* GetData() const;
  virtual size_t GetDataSize() const;

  // Returns a read-write pointer to the buffer data.
  virtual uint8* GetWritableData();

  // Updates the size of valid data in bytes, which must be less than or equal
  // to GetBufferSize().
  virtual void SetDataSize(size_t data_size);

  // Returns the size of the underlying buffer.
  virtual size_t GetBufferSize() const;

 protected:
  virtual ~DataBuffer();

 private:
  scoped_array<uint8> data_;
  size_t buffer_size_;
  size_t data_size_;

  DISALLOW_COPY_AND_ASSIGN(DataBuffer);
};

}  // namespace media

#endif  // MEDIA_BASE_DATA_BUFFER_H_
