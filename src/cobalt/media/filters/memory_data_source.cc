// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cobalt/media/filters/memory_data_source.h"

#include <algorithm>

#include "base/logging.h"

namespace media {

MemoryDataSource::MemoryDataSource(const uint8_t* data, size_t size)
    : data_(data), size_(size) {}

MemoryDataSource::~MemoryDataSource() {}

void MemoryDataSource::Read(int64_t position, int size, uint8_t* data,
                            const DataSource::ReadCB& read_cb) {
  DCHECK(!read_cb.is_null());

  if (is_stopped_ || size < 0 || position < 0 ||
      static_cast<size_t>(position) > size_) {
    read_cb.Run(kReadError);
    return;
  }

  // Cap size within bounds.
  size_t clamped_size = std::min(static_cast<size_t>(size),
                                 size_ - static_cast<size_t>(position));

  if (clamped_size > 0) {
    DCHECK(data);
    memcpy(data, data_ + position, clamped_size);
  }

  read_cb.Run(clamped_size);
}

void MemoryDataSource::Stop() { is_stopped_ = true; }

void MemoryDataSource::Abort() {}

bool MemoryDataSource::GetSize(int64_t* size_out) {
  *size_out = size_;
  return true;
}

bool MemoryDataSource::IsStreaming() { return false; }

void MemoryDataSource::SetBitrate(int bitrate) {}

}  // namespace media
