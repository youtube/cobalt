// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/io_buffer.h"

#include "base/logging.h"

namespace net {

IOBuffer::IOBuffer(int buffer_size) {
  DCHECK(buffer_size > 0);
  data_ = new char[buffer_size];
}

void DrainableIOBuffer::SetOffset(int bytes) {
  DCHECK(bytes >= 0 && bytes <= size_);
  used_ = bytes;
  data_ = base_->data() + used_;
}

bool GrowableIOBuffer::SetCapacity(int capacity) {
  DCHECK_GE(capacity, 0);
  char* cur_buf = real_data_.release();
  real_data_.reset(static_cast<char*>(realloc(cur_buf, capacity)));
  if (real_data_.get() == NULL && capacity != 0) {
    real_data_.reset(cur_buf);
    return false;
  }
  capacity_ = capacity;
  if (offset_ > capacity)
    set_offset(capacity);
  else
    set_offset(offset_);  // The pointer may have changed.
  return true;
}

void GrowableIOBuffer::set_offset(int offset) {
  DCHECK(offset >= 0 && offset <= capacity_);
  offset_ = offset;
  data_ = real_data_.get() + offset;
}

}  // namespace net
