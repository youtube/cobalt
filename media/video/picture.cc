// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/video/picture.h"

namespace media {

// PictureBuffer implementation.
PictureBuffer::PictureBuffer(int32 id,
                             gfx::Size pixel_size,
                             std::vector<uint32> color_format,
                             MemoryType memory_type,
                             std::vector<DataPlaneHandle> data_plane_handles)
    : id_(id),
      pixel_size_(pixel_size),
      color_format_(color_format),
      memory_type_(memory_type),
      data_plane_handles_(data_plane_handles) {
}

PictureBuffer::~PictureBuffer() {}

int32 PictureBuffer::GetId() {
  return id_;
}

gfx::Size PictureBuffer::GetSize() {
  return pixel_size_;
}

const std::vector<uint32>& PictureBuffer::GetColorFormat() {
  return color_format_;
}

PictureBuffer::MemoryType PictureBuffer::GetMemoryType() {
  return memory_type_;
}

std::vector<PictureBuffer::DataPlaneHandle>& PictureBuffer::GetPlaneHandles() {
  return data_plane_handles_;
}

// Picture implementation.
Picture::Picture(PictureBuffer* picture_buffer, gfx::Size decoded_pixel_size,
                 gfx::Size visible_pixel_size, void* user_handle)
    : picture_buffer_(picture_buffer),
      decoded_pixel_size_(decoded_pixel_size),
      visible_pixel_size_(visible_pixel_size),
      user_handle_(user_handle) {
}

Picture::~Picture() {}

PictureBuffer* Picture::picture_buffer() {
  return picture_buffer_;
}

gfx::Size Picture::GetDecodedSize() const {
  return decoded_pixel_size_;
}

gfx::Size Picture::GetVisibleSize() const {
  return visible_pixel_size_;
}

void* Picture::GetUserHandle() {
  return user_handle_;
}

}  // namespace media

