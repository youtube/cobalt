// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/logging.h"
#include "media/base/data_buffer.h"
#include "media/base/decrypt_config.h"
#if !defined(OS_ANDROID)
#include "media/ffmpeg/ffmpeg_common.h"
#endif

namespace media {

DataBuffer::DataBuffer(scoped_array<uint8> buffer, int buffer_size)
    : Buffer(base::TimeDelta(), base::TimeDelta()),
      data_(buffer.Pass()),
      buffer_size_(buffer_size),
      data_size_(buffer_size) {
}

DataBuffer::DataBuffer(int buffer_size)
    : Buffer(base::TimeDelta(), base::TimeDelta()),
      data_(new uint8[buffer_size]),
      buffer_size_(buffer_size),
      data_size_(0) {
  CHECK(data_.get()) << "DataBuffer ctor failed to allocate memory";

  // Prevent arbitrary pointers.
  if (buffer_size == 0)
    data_.reset(NULL);
}

DataBuffer::DataBuffer(const uint8* data, int data_size)
    : Buffer(base::TimeDelta(), base::TimeDelta()),
      buffer_size_(0),
      data_size_(0) {
  if (data_size == 0)
    return;

  int padding_size = 0;
#if !defined(OS_ANDROID)
  // FFmpeg assumes all input buffers are padded with this value.
  padding_size = FF_INPUT_BUFFER_PADDING_SIZE;
#endif

  buffer_size_ = data_size + padding_size;
  data_.reset(new uint8[buffer_size_]);
  memcpy(data_.get(), data, data_size);
  memset(data_.get() + data_size, 0, padding_size);
  SetDataSize(data_size);
}

DataBuffer::~DataBuffer() {}

scoped_refptr<DataBuffer> DataBuffer::CopyFrom(const uint8* data,
                                               int data_size) {
  return make_scoped_refptr(new DataBuffer(data, data_size));
}

const uint8* DataBuffer::GetData() const {
  return data_.get();
}

int DataBuffer::GetDataSize() const {
  return data_size_;
}

const DecryptConfig* DataBuffer::GetDecryptConfig() const {
  return decrypt_config_.get();
}

uint8* DataBuffer::GetWritableData() {
  return data_.get();
}

void DataBuffer::SetDataSize(int data_size) {
  DCHECK_LE(data_size, buffer_size_);
  data_size_ = data_size;
}

int DataBuffer::GetBufferSize() const {
  return buffer_size_;
}

void DataBuffer::SetDecryptConfig(scoped_ptr<DecryptConfig> decrypt_config) {
  decrypt_config_ = decrypt_config.Pass();
}

}  // namespace media
