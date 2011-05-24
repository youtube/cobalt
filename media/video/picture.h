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

// Information about the picture buffer.
// This is the media-namespace equivalent of PP_BufferInfo_Dev.
class BufferInfo {
 public:
  BufferInfo(int32 id, gfx::Size size);
  BufferInfo(const PP_BufferInfo_Dev& info);

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
class GLESBuffer {
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

  // Returns information regarding the buffer.
  const BufferInfo& buffer_info() const {
    return info_;
  }

 private:
  uint32 texture_id_;
  uint32 context_id_;
  BufferInfo info_;
};

// A picture buffer that lives in system memory.
// This is the media-namespace equivalent of PP_SysmemBuffer_Dev.
class SysmemBuffer {
 public:
  SysmemBuffer(int32 id, gfx::Size size, void* data);
  SysmemBuffer(const PP_SysmemBuffer_Dev&);

  // Returns a pointer to the buffer data.
  void* data() const {
    return data_;
  }

  // Returns information regarding the buffer.
  const BufferInfo& buffer_info() const {
    return info_;
  }

 private:
  void* data_;
  BufferInfo info_;
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
  // TODO(vrk): Handle the case where a picture can span multiple buffers.
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
