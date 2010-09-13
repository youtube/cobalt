// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/video_frame.h"

#include "base/logging.h"

namespace media {

static size_t GetNumberOfPlanes(VideoFrame::Format format) {
  switch (format) {
    case VideoFrame::RGB555:
    case VideoFrame::RGB565:
    case VideoFrame::RGB24:
    case VideoFrame::RGB32:
    case VideoFrame::RGBA:
    case VideoFrame::ASCII:
      return VideoFrame::kNumRGBPlanes;
    case VideoFrame::YV12:
    case VideoFrame::YV16:
      return VideoFrame::kNumYUVPlanes;
    case VideoFrame::NV12:
      return VideoFrame::kNumNV12Planes;
    default:
      return 0;
  }
}

// static
void VideoFrame::CreateFrame(VideoFrame::Format format,
                             size_t width,
                             size_t height,
                             base::TimeDelta timestamp,
                             base::TimeDelta duration,
                             scoped_refptr<VideoFrame>* frame_out) {
  DCHECK(width > 0 && height > 0);
  DCHECK(width * height < 100000000);
  DCHECK(frame_out);
  bool alloc_worked = false;
  scoped_refptr<VideoFrame> frame =
      new VideoFrame(VideoFrame::TYPE_SYSTEM_MEMORY, format, width, height);
  if (frame) {
    frame->SetTimestamp(timestamp);
    frame->SetDuration(duration);
    switch (format) {
      case VideoFrame::RGB555:
      case VideoFrame::RGB565:
        alloc_worked = frame->AllocateRGB(2u);
        break;
      case VideoFrame::RGB24:
        alloc_worked = frame->AllocateRGB(3u);
        break;
      case VideoFrame::RGB32:
      case VideoFrame::RGBA:
        alloc_worked = frame->AllocateRGB(4u);
        break;
      case VideoFrame::YV12:
      case VideoFrame::YV16:
        alloc_worked = frame->AllocateYUV();
        break;
      case VideoFrame::ASCII:
        alloc_worked = frame->AllocateRGB(1u);
        break;
      default:
        NOTREACHED();
        alloc_worked = false;
        break;
    }
  }
  *frame_out = alloc_worked ? frame : NULL;
}

// static
void VideoFrame::CreateFrameExternal(SurfaceType type,
                                     Format format,
                                     size_t width,
                                     size_t height,
                                     size_t planes,
                                     uint8* const data[kMaxPlanes],
                                     const int32 strides[kMaxPlanes],
                                     base::TimeDelta timestamp,
                                     base::TimeDelta duration,
                                     void* private_buffer,
                                     scoped_refptr<VideoFrame>* frame_out) {
  DCHECK(frame_out);
  scoped_refptr<VideoFrame> frame =
      new VideoFrame(type, format, width, height);
  if (frame) {
    frame->SetTimestamp(timestamp);
    frame->SetDuration(duration);
    frame->external_memory_ = true;
    frame->planes_ = planes;
    frame->private_buffer_ = private_buffer;
    for (size_t i = 0; i < kMaxPlanes; ++i) {
      frame->data_[i] = data[i];
      frame->strides_[i] = strides[i];
    }
  }
  *frame_out = frame;
}

// static
void VideoFrame::CreateFrameGlTexture(Format format,
                                      size_t width,
                                      size_t height,
                                      GlTexture const textures[kMaxPlanes],
                                      base::TimeDelta timestamp,
                                      base::TimeDelta duration,
                                      scoped_refptr<VideoFrame>* frame_out) {
  DCHECK(frame_out);
  scoped_refptr<VideoFrame> frame =
      new VideoFrame(TYPE_GL_TEXTURE, format, width, height);
  if (frame) {
    frame->SetTimestamp(timestamp);
    frame->SetDuration(duration);
    frame->external_memory_ = true;
    frame->planes_ = GetNumberOfPlanes(format);
    for (size_t i = 0; i < kMaxPlanes; ++i) {
      frame->gl_textures_[i] = textures[i];
    }
  }
  *frame_out = frame;
}

// static
void VideoFrame::CreateFrameD3dTexture(Format format,
                                       size_t width,
                                       size_t height,
                                       D3dTexture const textures[kMaxPlanes],
                                       base::TimeDelta timestamp,
                                       base::TimeDelta duration,
                                       scoped_refptr<VideoFrame>* frame_out) {
  DCHECK(frame_out);
  scoped_refptr<VideoFrame> frame =
      new VideoFrame(TYPE_D3D_TEXTURE, format, width, height);
  if (frame) {
    frame->SetTimestamp(timestamp);
    frame->SetDuration(duration);
    frame->external_memory_ = true;
    frame->planes_ = GetNumberOfPlanes(format);
    for (size_t i = 0; i < kMaxPlanes; ++i) {
      frame->d3d_textures_[i] = textures[i];
    }
  }
  *frame_out = frame;
}

// static
void VideoFrame::CreateEmptyFrame(scoped_refptr<VideoFrame>* frame_out) {
  *frame_out = new VideoFrame(VideoFrame::TYPE_SYSTEM_MEMORY,
                              VideoFrame::EMPTY, 0, 0);
}

// static
void VideoFrame::CreateBlackFrame(int width, int height,
                                  scoped_refptr<VideoFrame>* frame_out) {
  DCHECK_GT(width, 0);
  DCHECK_GT(height, 0);

  // Create our frame.
  scoped_refptr<VideoFrame> frame;
  const base::TimeDelta kZero;
  VideoFrame::CreateFrame(VideoFrame::YV12, width, height, kZero, kZero,
                          &frame);
  DCHECK(frame);

  // Now set the data to YUV(0,128,128).
  const uint8 kBlackY = 0x00;
  const uint8 kBlackUV = 0x80;

  // Fill the Y plane.
  uint8* y_plane = frame->data(VideoFrame::kYPlane);
  for (size_t i = 0; i < frame->height_; ++i) {
    memset(y_plane, kBlackY, frame->width_);
    y_plane += frame->stride(VideoFrame::kYPlane);
  }

  // Fill the U and V planes.
  uint8* u_plane = frame->data(VideoFrame::kUPlane);
  uint8* v_plane = frame->data(VideoFrame::kVPlane);
  for (size_t i = 0; i < (frame->height_ / 2); ++i) {
    memset(u_plane, kBlackUV, frame->width_ / 2);
    memset(v_plane, kBlackUV, frame->width_ / 2);
    u_plane += frame->stride(VideoFrame::kUPlane);
    v_plane += frame->stride(VideoFrame::kVPlane);
  }

  // Success!
  *frame_out = frame;
}

static inline size_t RoundUp(size_t value, size_t alignment) {
  // Check that |alignment| is a power of 2.
  DCHECK((alignment + (alignment - 1)) == (alignment | (alignment - 1)));
  return ((value + (alignment - 1)) & ~(alignment-1));
}

bool VideoFrame::AllocateRGB(size_t bytes_per_pixel) {
  // Round up to align at a 64-bit (8 byte) boundary for each row.  This
  // is sufficient for MMX reads (movq).
  size_t bytes_per_row = RoundUp(width_ * bytes_per_pixel, 8);
  planes_ = VideoFrame::kNumRGBPlanes;
  strides_[VideoFrame::kRGBPlane] = bytes_per_row;
  data_[VideoFrame::kRGBPlane] = new uint8[bytes_per_row * height_];
  DCHECK(data_[VideoFrame::kRGBPlane]);
  DCHECK(!(reinterpret_cast<intptr_t>(data_[VideoFrame::kRGBPlane]) & 7));
  COMPILE_ASSERT(0 == VideoFrame::kRGBPlane, RGB_data_must_be_index_0);
  return (NULL != data_[VideoFrame::kRGBPlane]);
}

static const int kFramePadBytes = 15;  // allows faster SIMD YUV convert

bool VideoFrame::AllocateYUV() {
  DCHECK(format_ == VideoFrame::YV12 ||
         format_ == VideoFrame::YV16);
  // Align Y rows at 32-bit (4 byte) boundaries.  The stride for both YV12 and
  // YV16 is 1/2 of the stride of Y.  For YV12, every row of bytes for U and V
  // applies to two rows of Y (one byte of UV for 4 bytes of Y), so in the
  // case of YV12 the strides are identical for the same width surface, but the
  // number of bytes allocated for YV12 is 1/2 the amount for U & V as YV16.
  // We also round the height of the surface allocated to be an even number
  // to avoid any potential of faulting by code that attempts to access the Y
  // values of the final row, but assumes that the last row of U & V applies to
  // a full two rows of Y.
  size_t alloc_height = RoundUp(height_, 2);
  size_t y_bytes_per_row = RoundUp(width_, 4);
  size_t uv_stride = RoundUp(y_bytes_per_row / 2, 4);
  size_t y_bytes = alloc_height * y_bytes_per_row;
  size_t uv_bytes = alloc_height * uv_stride;
  if (format_ == VideoFrame::YV12) {
    uv_bytes /= 2;
  }
  uint8* data = new uint8[y_bytes + (uv_bytes * 2) + kFramePadBytes];
  if (data) {
    planes_ = VideoFrame::kNumYUVPlanes;
    COMPILE_ASSERT(0 == VideoFrame::kYPlane, y_plane_data_must_be_index_0);
    data_[VideoFrame::kYPlane] = data;
    data_[VideoFrame::kUPlane] = data + y_bytes;
    data_[VideoFrame::kVPlane] = data + y_bytes + uv_bytes;
    strides_[VideoFrame::kYPlane] = y_bytes_per_row;
    strides_[VideoFrame::kUPlane] = uv_stride;
    strides_[VideoFrame::kVPlane] = uv_stride;
    return true;
  }
  NOTREACHED();
  return false;
}

VideoFrame::VideoFrame(VideoFrame::SurfaceType type,
                       VideoFrame::Format format,
                       size_t width,
                       size_t height) {
  type_ = type;
  format_ = format;
  width_ = width;
  height_ = height;
  planes_ = 0;
  memset(&strides_, 0, sizeof(strides_));
  memset(&data_, 0, sizeof(data_));
  memset(&gl_textures_, 0, sizeof(gl_textures_));
  memset(&d3d_textures_, 0, sizeof(d3d_textures_));
  external_memory_ = false;
  private_buffer_ = NULL;
}

VideoFrame::~VideoFrame() {
  // In multi-plane allocations, only a single block of memory is allocated
  // on the heap, and other |data| pointers point inside the same, single block
  // so just delete index 0.
  if (!external_memory_)
    delete[] data_[0];
}

bool VideoFrame::IsEndOfStream() const {
  return format_ == VideoFrame::EMPTY;
}

}  // namespace media
