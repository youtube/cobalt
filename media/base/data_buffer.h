// Copyright (c) 2012 The Chromium Authors. All rights reserved.
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
  // Assumes valid data of size |buffer_size|.
  DataBuffer(scoped_array<uint8> buffer, int buffer_size);

  // Allocates buffer of size |buffer_size|. If |buffer_size| is 0, |data_| is
  // set to a NULL ptr.
  explicit DataBuffer(int buffer_size);

  // Allocates buffer of size |data_size|, copies [data,data+data_size) to
  // the allocated buffer and sets data size to |data_size|.
  DataBuffer(const uint8* data, int data_size);

  // Buffer implementation.
  virtual const uint8* GetData() const override;
  virtual int GetDataSize() const override;

  // Returns a read-write pointer to the buffer data.
  virtual uint8* GetWritableData();

  // Updates the size of valid data in bytes, which must be less than or equal
  // to GetBufferSize().
  virtual void SetDataSize(int data_size);

  // Returns the size of the underlying buffer.
  virtual int GetBufferSize() const;

 protected:
  virtual ~DataBuffer();

 private:
  // Constructor helper method for memory allocations.
  void Initialize();

  scoped_array<uint8> data_;
  int buffer_size_;
  int data_size_;

  DISALLOW_COPY_AND_ASSIGN(DataBuffer);
};

}  // namespace media

#endif  // MEDIA_BASE_DATA_BUFFER_H_
