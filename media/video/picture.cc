// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/video/picture.h"

namespace media {

// Implementations for the other constructors are found in
// webkit/plugins/ppapi/ppb_video_decoder_impl.cc.
// They are not included in this file because it would require
// media/ to depend on files in ppapi/.
BufferInfo::BufferInfo(int32 id, gfx::Size size)
    : id_(id),
      size_(size) {
}

GLESBuffer::GLESBuffer(
    int32 id, gfx::Size size, uint32 texture_id, uint32 context_id)
    : texture_id_(texture_id),
      context_id_(context_id),
      info_(id, size) {
}

SysmemBuffer::SysmemBuffer(int32 id, gfx::Size size, void* data)
    : data_(data),
      info_(id, size) {
}

Picture::Picture(int32 picture_buffer_id, int32 bitstream_buffer_id,
                 gfx::Size visible_size, gfx::Size decoded_size)
    : picture_buffer_id_(picture_buffer_id),
      bitstream_buffer_id_(bitstream_buffer_id),
      visible_size_(visible_size),
      decoded_size_(decoded_size) {
}

}  // namespace media
