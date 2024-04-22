// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/codec/vector_wstream.h"

namespace gfx {

bool VectorWStream::write(const void* buffer, size_t size) {
  const unsigned char* ptr = reinterpret_cast<const unsigned char*>(buffer);
  dst_->insert(dst_->end(), ptr, ptr + size);
  return true;
}

size_t VectorWStream::bytesWritten() const {
  return dst_->size();
}

}  // namespace gfx
