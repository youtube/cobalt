// Copyright (c) 2008-2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// A simple implementation of WritableBuffer that takes ownership of
// the given data pointer.
//
// DataBuffer assumes that memory was allocated with new uint8[].

#ifndef MEDIA_BASE_DATA_BUFFER_H_
#define MEDIA_BASE_DATA_BUFFER_H_

#include "base/scoped_ptr.h"
#include "media/base/buffers.h"

namespace media {

class DataBuffer : public WritableBuffer {
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

  // WritableBuffer implementation.
  virtual uint8* GetWritableData();
  virtual void SetDataSize(size_t data_size);
  virtual size_t GetBufferSize() const;

 protected:
  virtual ~DataBuffer();

 private:
  scoped_array<uint8> data_;
  size_t buffer_size_;
  size_t data_size_;
};

}  // namespace media

#endif  // MEDIA_BASE_DATA_BUFFER_H_
