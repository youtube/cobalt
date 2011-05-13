// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_BITSTREAM_BUFFER_H_
#define MEDIA_BASE_BITSTREAM_BUFFER_H_

#include "base/basictypes.h"

namespace media {

// Class for passing bitstream buffers around. Ownership of the bitstream
// pointer remains with whoever uses this class.
// This is media-namespace equivalent of PP_BitstreamBuffer_Dev.
class BitstreamBuffer {
 public:
  BitstreamBuffer(int32 id, uint8* data, size_t size)
      : id_(id),
        data_(data),
        size_(size) {
  }

  int32 id() const { return id_; }
  uint8* data() const { return data_; }
  size_t size() const { return size_; }

 private:
  int32 id_;
  uint8* data_;
  size_t size_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(BitstreamBuffer);
};

}  // namespace media

#endif  // MEDIA_BASE_BITSTREAM_BUFFER_H_
