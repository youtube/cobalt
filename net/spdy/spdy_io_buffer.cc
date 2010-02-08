// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/spdy/spdy_io_buffer.h"
#include "net/spdy/spdy_stream.h"

namespace net {

// static
uint64 SpdyIOBuffer::order_ = 0;

SpdyIOBuffer::SpdyIOBuffer(
    IOBuffer* buffer, int size, int priority, SpdyStream* stream)
  : buffer_(new DrainableIOBuffer(buffer, size)),
    priority_(priority),
    position_(++order_),
    stream_(stream) {}

SpdyIOBuffer::SpdyIOBuffer() : priority_(0), position_(0), stream_(NULL) {}

SpdyIOBuffer::~SpdyIOBuffer() {}

void SpdyIOBuffer::release() {
  buffer_ = NULL;
  stream_ = NULL;
}

}  // namespace net
