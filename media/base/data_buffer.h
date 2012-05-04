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

  // Create a DataBuffer whose |data_| is copied from |data|.
  static scoped_refptr<DataBuffer> CopyFrom(const uint8* data, int size);

  // Buffer implementation.
  virtual const uint8* GetData() const OVERRIDE;
  virtual int GetDataSize() const OVERRIDE;
  virtual const DecryptConfig* GetDecryptConfig() const OVERRIDE;

  // Returns a read-write pointer to the buffer data.
  virtual uint8* GetWritableData();

  // Updates the size of valid data in bytes, which must be less than or equal
  // to GetBufferSize().
  virtual void SetDataSize(int data_size);

  // Returns the size of the underlying buffer.
  virtual int GetBufferSize() const;

  virtual void SetDecryptConfig(scoped_ptr<DecryptConfig> decrypt_config);

 protected:
  // Copies from [data,data+size) to owned array.
  DataBuffer(const uint8* data, int size);
  virtual ~DataBuffer();

 private:
  // Helper method to allocate |data_| with at least |buffer_size| bytes.
  void AllocateBuffer(int buffer_size);

  scoped_array<uint8> data_;
  int buffer_size_;
  int data_size_;
  scoped_ptr<DecryptConfig> decrypt_config_;

  DISALLOW_COPY_AND_ASSIGN(DataBuffer);
};

}  // namespace media

#endif  // MEDIA_BASE_DATA_BUFFER_H_
