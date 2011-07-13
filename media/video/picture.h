// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_VIDEO_PICTURE_H_
#define MEDIA_VIDEO_PICTURE_H_

#include "base/basictypes.h"
#include "ui/gfx/gl/gl_context.h"
#include "ui/gfx/size.h"

namespace media {

// Common information about GLES & Sysmem picture buffers.
// This is the media-namespace equivalent of PP_BufferInfo_Dev.
class BaseBuffer {
 public:
  BaseBuffer(int32 id, gfx::Size size);
  virtual ~BaseBuffer();

  // Returns the client-specified id of the buffer.
  int32 id() const {
    return id_;
  }

  // Returns the size of the buffer.
  gfx::Size size() const {
    return size_;
  }

 private:
  int32 id_;
  gfx::Size size_;
};

// A picture buffer that is composed of a GLES2 texture and context.
// This is the media-namespace equivalent of PP_GLESBuffer_Dev.
class GLESBuffer : public BaseBuffer {
 public:
  GLESBuffer(int32 id, gfx::Size size, uint32 texture_id);

  // Returns the id of the texture.
  // NOTE: The texture id in the renderer process corresponds to a different
  // texture id in the GPU process.
  uint32 texture_id() const {
    return texture_id_;
  }

 private:
  uint32 texture_id_;
};

// TODO(fischman,vrk): rip out SysmemBuffer and all its vestiges (such as
// BaseBuffer's existence, VideoDecodeAccelerator::MemoryType, their ppapi
// equivalents, etc).  Rename GLESBuffer to PictureBuffer.

// A picture buffer that lives in system memory.
// This is the media-namespace equivalent of PP_SysmemBuffer_Dev.
class SysmemBuffer : public BaseBuffer {
 public:
  SysmemBuffer(int32 id, gfx::Size size, void* data);

  // Returns a pointer to the buffer data.
  void* data() const {
    return data_;
  }

 private:
  void* data_;
};

// A decoded picture frame.
// This is the media-namespace equivalent of PP_Picture_Dev.
class Picture {
 public:
  Picture(int32 picture_buffer_id, int32 bitstream_buffer_id,
          gfx::Size visible_size, gfx::Size decoded_size);

  // Returns the id of the picture buffer where this picture is contained.
  int32 picture_buffer_id() const {
    return picture_buffer_id_;
  }

  // Returns the id of the bitstream buffer from which this frame was decoded.
  int32 bitstream_buffer_id() const {
    return bitstream_buffer_id_;
  }

  void set_bitstream_buffer_id(int32 bitstream_buffer_id) {
    bitstream_buffer_id_ = bitstream_buffer_id;
  }

  // Returns the visible size of the decoded picture in pixels.
  gfx::Size visible_size() const {
    return visible_size_;
  }

  // Returns the decoded size of the decoded picture in pixels.
  gfx::Size decoded_size() const {
    return decoded_size_;
  }

 private:
  int32 picture_buffer_id_;
  int32 bitstream_buffer_id_;
  gfx::Size visible_size_;
  gfx::Size decoded_size_;
};

}  // namespace media

#endif  // MEDIA_VIDEO_PICTURE_H_
