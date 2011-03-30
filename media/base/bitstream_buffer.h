// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_BITSTREAM_BUFFER_H_
#define MEDIA_BASE_BITSTREAM_BUFFER_H_

#include "base/basictypes.h"

namespace media {

// Class for passing bitstream buffers around. Ownership of the bitstream
// pointer remains with whoever uses this class.
class BitstreamBuffer {
 public:
  BitstreamBuffer(uint8* bitstream, size_t bitstream_size, void* user_handle)
      : bitstream_(bitstream),
        bitstream_size_(bitstream_size),
        user_handle_(user_handle) {
  }

  ~BitstreamBuffer() {}

  uint8* bitstream() { return bitstream_; }
  size_t bitstream_size() { return bitstream_size_; }
  void* user_handle() { return user_handle_; }

 private:
  uint8* bitstream_;
  size_t bitstream_size_;
  void* user_handle_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(BitstreamBuffer);
};

}  // namespace media

#endif  // MEDIA_BASE_BITSTREAM_BUFFER_H_
