// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/video_frame.h"

#include "base/logging.h"
#include "base/string_piece.h"
#include "media/base/limits.h"
#include "media/base/video_util.h"
#if !defined(OS_ANDROID)
#include "media/ffmpeg/ffmpeg_common.h"
#endif

#include <algorithm>

namespace media {

// static
scoped_refptr<VideoFrame> VideoFrame::CreateFrame(
    VideoFrame::Format format,
    size_t width,
    size_t height,
    base::TimeDelta timestamp,
    base::TimeDelta duration) {
  DCHECK(IsValidConfig(format, width, height));
  scoped_refptr<VideoFrame> frame(new VideoFrame(
      format, width, height, timestamp, duration));
  switch (format) {
    case VideoFrame::RGB32:
      frame->AllocateRGB(4u);
      break;
    case VideoFrame::YV12:
    case VideoFrame::YV16:
      frame->AllocateYUV();
      break;
    default:
      LOG(FATAL) << "Unsupported frame format: " << format;
  }
  return frame;
}

// static
bool VideoFrame::IsValidConfig(
    VideoFrame::Format format,
    size_t width,
    size_t height) {

  return (format != VideoFrame::INVALID &&
          width > 0 && height > 0 &&
          width <= limits::kMaxDimension && height <= limits::kMaxDimension &&
          width * height <= limits::kMaxCanvas);
}

// static
scoped_refptr<VideoFrame> VideoFrame::WrapNativeTexture(
    uint32 texture_id,
    uint32 texture_target,
    size_t width,
    size_t height,
    base::TimeDelta timestamp,
    base::TimeDelta duration,
    const base::Closure& no_longer_needed) {
  scoped_refptr<VideoFrame> frame(
      new VideoFrame(NATIVE_TEXTURE, width, height, timestamp, duration));
  frame->texture_id_ = texture_id;
  frame->texture_target_ = texture_target;
  frame->texture_no_longer_needed_ = no_longer_needed;
  return frame;
}

// static
scoped_refptr<VideoFrame> VideoFrame::CreateEmptyFrame() {
  return new VideoFrame(
      VideoFrame::EMPTY, 0, 0, base::TimeDelta(), base::TimeDelta());
}

// static
scoped_refptr<VideoFrame> VideoFrame::CreateBlackFrame(int width, int height) {
  DCHECK_GT(width, 0);
  DCHECK_GT(height, 0);

  // Create our frame.
  const base::TimeDelta kZero;
  scoped_refptr<VideoFrame> frame =
      VideoFrame::CreateFrame(VideoFrame::YV12, width, height, kZero, kZero);

  // Now set the data to YUV(0,128,128).
  const uint8 kBlackY = 0x00;
  const uint8 kBlackUV = 0x80;
  FillYUV(frame, kBlackY, kBlackUV, kBlackUV);
  return frame;
}

static inline size_t RoundUp(size_t value, size_t alignment) {
  // Check that |alignment| is a power of 2.
  DCHECK((alignment + (alignment - 1)) == (alignment | (alignment - 1)));
  return ((value + (alignment - 1)) & ~(alignment-1));
}

static const int kFrameSizeAlignment = 16;

void VideoFrame::AllocateRGB(size_t bytes_per_pixel) {
  // Round up to align at least at a 16-byte boundary for each row.
  // This is sufficient for MMX and SSE2 reads (movq/movdqa).
  size_t bytes_per_row = RoundUp(width_, kFrameSizeAlignment) * bytes_per_pixel;
  size_t aligned_height = RoundUp(height_, kFrameSizeAlignment);
  strides_[VideoFrame::kRGBPlane] = bytes_per_row;
#if !defined(OS_ANDROID)
  // TODO(dalecurtis): use DataAligned or so, so this #ifdef hackery
  // doesn't need to be repeated in every single user of aligned data.
  data_[VideoFrame::kRGBPlane] = reinterpret_cast<uint8*>(
      av_malloc(bytes_per_row * aligned_height));
#else
  data_[VideoFrame::kRGBPlane] = new uint8_t[bytes_per_row * aligned_height];
#endif
  DCHECK(!(reinterpret_cast<intptr_t>(data_[VideoFrame::kRGBPlane]) & 7));
  COMPILE_ASSERT(0 == VideoFrame::kRGBPlane, RGB_data_must_be_index_0);
}

void VideoFrame::AllocateYUV() {
  DCHECK(format_ == VideoFrame::YV12 || format_ == VideoFrame::YV16);
  // Align Y rows at least at 16 byte boundaries.  The stride for both
  // YV12 and YV16 is 1/2 of the stride of Y.  For YV12, every row of bytes for
  // U and V applies to two rows of Y (one byte of UV for 4 bytes of Y), so in
  // the case of YV12 the strides are identical for the same width surface, but
  // the number of bytes allocated for YV12 is 1/2 the amount for U & V as
  // YV16. We also round the height of the surface allocated to be an even
  // number to avoid any potential of faulting by code that attempts to access
  // the Y values of the final row, but assumes that the last row of U & V
  // applies to a full two rows of Y.
  size_t y_stride = RoundUp(row_bytes(VideoFrame::kYPlane),
                            kFrameSizeAlignment);
  size_t uv_stride = RoundUp(row_bytes(VideoFrame::kUPlane),
                             kFrameSizeAlignment);
  size_t y_height = RoundUp(height_, kFrameSizeAlignment);
  size_t uv_height = format_ == VideoFrame::YV12 ? y_height / 2 : y_height;
  size_t y_bytes = y_height * y_stride;
  size_t uv_bytes = uv_height * uv_stride;

#if !defined(OS_ANDROID)
  // TODO(dalecurtis): use DataAligned or so, so this #ifdef hackery
  // doesn't need to be repeated in every single user of aligned data.
  uint8* data = reinterpret_cast<uint8*>(
      av_malloc(y_bytes + (uv_bytes * 2)));
#else
  uint8* data = new uint8_t[y_bytes + (uv_bytes * 2)];
#endif
  COMPILE_ASSERT(0 == VideoFrame::kYPlane, y_plane_data_must_be_index_0);
  data_[VideoFrame::kYPlane] = data;
  data_[VideoFrame::kUPlane] = data + y_bytes;
  data_[VideoFrame::kVPlane] = data + y_bytes + uv_bytes;
  strides_[VideoFrame::kYPlane] = y_stride;
  strides_[VideoFrame::kUPlane] = uv_stride;
  strides_[VideoFrame::kVPlane] = uv_stride;
}

VideoFrame::VideoFrame(VideoFrame::Format format,
                       size_t width,
                       size_t height,
                       base::TimeDelta timestamp,
                       base::TimeDelta duration)
    : format_(format),
      width_(width),
      height_(height),
      texture_id_(0),
      texture_target_(0),
      timestamp_(timestamp),
      duration_(duration) {
  memset(&strides_, 0, sizeof(strides_));
  memset(&data_, 0, sizeof(data_));
}

VideoFrame::~VideoFrame() {
  if (format_ == NATIVE_TEXTURE && !texture_no_longer_needed_.is_null()) {
    texture_no_longer_needed_.Run();
    texture_no_longer_needed_.Reset();
  }

  // In multi-plane allocations, only a single block of memory is allocated
  // on the heap, and other |data| pointers point inside the same, single block
  // so just delete index 0.
  if (data_[0]) {
#if !defined(OS_ANDROID)
    av_free(data_[0]);
#else
    delete[] data_[0];
#endif
  }
}

bool VideoFrame::IsValidPlane(size_t plane) const {
  switch (format_) {
    case RGB32:
      return plane == kRGBPlane;

    case YV12:
    case YV16:
      return plane == kYPlane || plane == kUPlane || plane == kVPlane;

    case NATIVE_TEXTURE:
      NOTREACHED() << "NATIVE_TEXTUREs don't use plane-related methods!";
      return false;

    default:
      break;
  }

  // Intentionally leave out non-production formats.
  NOTREACHED() << "Unsupported video frame format: " << format_;
  return false;
}

int VideoFrame::stride(size_t plane) const {
  DCHECK(IsValidPlane(plane));
  return strides_[plane];
}

int VideoFrame::row_bytes(size_t plane) const {
  DCHECK(IsValidPlane(plane));
  switch (format_) {
    // 32bpp.
    case RGB32:
      return width_ * 4;

    // Planar, 8bpp.
    case YV12:
    case YV16:
      if (plane == kYPlane)
        return width_;
      return RoundUp(width_, 2) / 2;

    default:
      break;
  }

  // Intentionally leave out non-production formats.
  NOTREACHED() << "Unsupported video frame format: " << format_;
  return 0;
}

int VideoFrame::rows(size_t plane) const {
  DCHECK(IsValidPlane(plane));
  switch (format_) {
    case RGB32:
    case YV16:
      return height_;

    case YV12:
      if (plane == kYPlane)
        return height_;
      return RoundUp(height_, 2) / 2;

    default:
      break;
  }

  // Intentionally leave out non-production formats.
  NOTREACHED() << "Unsupported video frame format: " << format_;
  return 0;
}

uint8* VideoFrame::data(size_t plane) const {
  DCHECK(IsValidPlane(plane));
  return data_[plane];
}

uint32 VideoFrame::texture_id() const {
  DCHECK_EQ(format_, NATIVE_TEXTURE);
  return texture_id_;
}

uint32 VideoFrame::texture_target() const {
  DCHECK_EQ(format_, NATIVE_TEXTURE);
  return texture_target_;
}

bool VideoFrame::IsEndOfStream() const {
  return format_ == VideoFrame::EMPTY;
}

void VideoFrame::HashFrameForTesting(base::MD5Context* context) {
  for(int plane = 0; plane < kMaxPlanes; plane++) {
    if (!IsValidPlane(plane))
      break;
    for(int row = 0; row < rows(plane); row++) {
      base::MD5Update(context, base::StringPiece(
          reinterpret_cast<char*>(data(plane) + stride(plane) * row),
          row_bytes(plane)));
    }
  }
}

}  // namespace media
