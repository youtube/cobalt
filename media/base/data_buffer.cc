// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/logging.h"
#include "media/base/data_buffer.h"

namespace media {

DataBuffer::DataBuffer(uint8* buffer, size_t buffer_size)
    : data_(buffer),
      buffer_size_(buffer_size),
      data_size_(buffer_size) {
}

DataBuffer::DataBuffer(size_t buffer_size)
    : data_(new uint8[buffer_size]),
      buffer_size_(buffer_size),
      data_size_(0) {
  CHECK(data_.get()) << "DataBuffer ctor failed to allocate memory";

  // Prevent arbitrary pointers.
  if (buffer_size == 0)
    data_.reset(NULL);
}

DataBuffer::~DataBuffer() {
}

const uint8* DataBuffer::GetData() const {
  return data_.get();
}

size_t DataBuffer::GetDataSize() const {
  return data_size_;
}

uint8* DataBuffer::GetWritableData() {
  return data_.get();
}


void DataBuffer::SetDataSize(size_t data_size) {
  DCHECK_LE(data_size, buffer_size_);
  data_size_ = data_size;
}

size_t DataBuffer::GetBufferSize() const {
  return buffer_size_;
}

}  // namespace media
