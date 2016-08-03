/*
 * Copyright 2015 Google Inc. All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef MEDIA_BASE_SHELL_VIDEO_DATA_ALLOCATOR_H_
#define MEDIA_BASE_SHELL_VIDEO_DATA_ALLOCATOR_H_

#include "base/compiler_specific.h"
#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "media/base/video_frame.h"

namespace media {

class ShellRawVideoDecoder;

// This class is introduced to remove the hidden dependency on the platform
// dependent graphics code from low level video decoders. It is possible that
// this can be achieved via interfaces created on each platform. However, to
// abstract them into a common interface is more explicit.
class MEDIA_EXPORT ShellVideoDataAllocator {
 public:
  class FrameBuffer : public base::RefCountedThreadSafe<FrameBuffer> {
   public:
    FrameBuffer() {}
    virtual ~FrameBuffer() {}
    virtual uint8* data() const = 0;
    virtual size_t size() const = 0;

   private:
    DISALLOW_COPY_AND_ASSIGN(FrameBuffer);
  };

  class YV12Param {
   public:
    YV12Param(int decoded_width,
              int decoded_height,
              const gfx::Rect& visible_rect,
              uint8* data);

    // Create with data pointer to individual planes. All pointers should be in
    // the same memory block controlled by the accompanied FrameBuffer passed to
    // CreateYV12Frame. The decoded size and visible rect are assumed to be the
    // same. It is only used for decoding vp9 frames.
    YV12Param(int width,
              int height,
              int y_pitch,
              int uv_pitch,
              uint8* y_data,
              uint8* u_data,
              uint8* v_data);
    int y_pitch() const {
      DCHECK_NE(y_pitch_, 0);
      return y_pitch_;
    }
    int uv_pitch() const {
      DCHECK_NE(uv_pitch_, 0);
      return uv_pitch_;
    }
    uint8* y_data() const {
      DCHECK(y_data_);
      return y_data_;
    }
    uint8* u_data() const {
      DCHECK(u_data_);
      return u_data_;
    }
    uint8* v_data() const {
      DCHECK(v_data_);
      return v_data_;
    }

    int decoded_width() const { return decoded_width_; }
    int decoded_height() const { return decoded_height_; }
    const gfx::Rect& visible_rect() const { return visible_rect_; }

   private:
    int decoded_width_;
    int decoded_height_;

    gfx::Rect visible_rect_;

    int y_pitch_;
    int uv_pitch_;
    uint8* y_data_;
    uint8* u_data_;
    uint8* v_data_;
  };

  // Only used for some platforms' hardware AVC decoder that only support NV12
  // output.
  class NV12Param {
   public:
    NV12Param(int decoded_width,
              int decoded_height,
              int y_pitch,
              const gfx::Rect& visible_rect);

    int decoded_width() const { return decoded_width_; }
    int decoded_height() const { return decoded_height_; }
    int y_pitch() const { return y_pitch_; }
    const gfx::Rect& visible_rect() const { return visible_rect_; }

   private:
    int decoded_width_;
    int decoded_height_;
    int y_pitch_;
    gfx::Rect visible_rect_;
  };

  ShellVideoDataAllocator() {}
  virtual ~ShellVideoDataAllocator() {}

  // Allocate a buffer to store the video frame to be decoded.
  virtual scoped_refptr<FrameBuffer> AllocateFrameBuffer(size_t size,
                                                         size_t alignment) = 0;
  virtual scoped_refptr<VideoFrame> CreateYV12Frame(
      const scoped_refptr<FrameBuffer>& frame_buffer,
      const YV12Param& param,
      const base::TimeDelta& timestamp) = 0;

  // Some hardware AVC decoders only support NV12 output. They are perfectly
  // aligned for rendering as texture though.
  virtual scoped_refptr<VideoFrame> CreateNV12Frame(
      const scoped_refptr<FrameBuffer>& frame_buffer,
      const NV12Param& param,
      const base::TimeDelta& timestamp) {
    UNREFERENCED_PARAMETER(frame_buffer);
    UNREFERENCED_PARAMETER(param);
    UNREFERENCED_PARAMETER(timestamp);
    NOTREACHED();
    return NULL;
  }

  // Return a video frame filled with RGBA zero on platforms that require punch
  // out.  Return NULL otherwise.
  virtual scoped_refptr<VideoFrame> GetPunchOutFrame() { return NULL; }

  // Most platforms limit the number of active raw video decoders to one.  The
  // following functions enable the implementations on these platforms to check
  // if there is more than one video decoder active.
  virtual void Acquire(ShellRawVideoDecoder* owner) {
    UNREFERENCED_PARAMETER(owner);
  }
  virtual void Release(ShellRawVideoDecoder* owner) {
    UNREFERENCED_PARAMETER(owner);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(ShellVideoDataAllocator);
};

}  // namespace media

#endif  // MEDIA_BASE_SHELL_VIDEO_DATA_ALLOCATOR_H_
