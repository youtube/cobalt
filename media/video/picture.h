// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_VIDEO_PICTURE_H_
#define MEDIA_VIDEO_PICTURE_H_

#include "base/basictypes.h"
#include "ui/gfx/size.h"

struct PP_BufferInfo_Dev;
struct PP_GLESBuffer_Dev;
struct PP_Picture_Dev;
struct PP_SysmemBuffer_Dev;

namespace media {

// Common information about GLES & Sysmem picture buffers.
// This is the media-namespace equivalent of PP_BufferInfo_Dev.
class BaseBuffer {
 public:
  BaseBuffer(int32 id, gfx::Size size);
  BaseBuffer(const PP_BufferInfo_Dev& info);
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
  GLESBuffer(int32 id, gfx::Size size, uint32 texture_id, uint32 context_id);
  GLESBuffer(const PP_GLESBuffer_Dev& buffer);

  // Returns the id of the texture.
  uint32 texture_id() const {
    return texture_id_;
  }

  // Returns the id of the context in which this texture lives.
  // TODO(vrk): I'm not sure if "id" is what we want, or some reference to the
  // PPB_Context3D_Dev. Not sure how this eventually gets used.
  uint32 context_id() const {
    return context_id_;
  }

 private:
  uint32 texture_id_;
  uint32 context_id_;
};

// A picture buffer that lives in system memory.
// This is the media-namespace equivalent of PP_SysmemBuffer_Dev.
class SysmemBuffer : public BaseBuffer {
 public:
  SysmemBuffer(int32 id, gfx::Size size, void* data);
  SysmemBuffer(const PP_SysmemBuffer_Dev&);

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
  Picture(const PP_Picture_Dev& picture);

  // Returns the id of the picture buffer where this picture is contained.
  int32 picture_buffer_id() const {
    return picture_buffer_id_;
  }

  // Returns the id of the bitstream buffer from which this frame was decoded.
  // TODO(fischman,vrk): Remove this field; pictures can span arbitrarily many
  // BitstreamBuffers, and it's not clear what clients would do with this
  // information, anyway.
  int32 bitstream_buffer_id() const {
    return bitstream_buffer_id_;
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
