// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/video_frame.h"

#include <algorithm>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/logging.h"
#include "base/string_piece.h"
#include "media/base/limits.h"
#include "media/base/video_util.h"
#if !defined(OS_ANDROID)
#include "media/ffmpeg/ffmpeg_common.h"
#endif

namespace media {

// static
scoped_refptr<VideoFrame> VideoFrame::CreateFrame(
    VideoFrame::Format format,
    const gfx::Size& coded_size,
    const gfx::Rect& visible_rect,
    const gfx::Size& natural_size,
    base::TimeDelta timestamp) {
  DCHECK(IsValidConfig(format, coded_size, visible_rect, natural_size));
  scoped_refptr<VideoFrame> frame(new VideoFrame(
      format, coded_size, visible_rect, natural_size, timestamp));
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
bool VideoFrame::IsValidConfig(VideoFrame::Format format,
                               const gfx::Size& coded_size,
                               const gfx::Rect& visible_rect,
                               const gfx::Size& natural_size) {
  return (format != VideoFrame::INVALID &&
          !coded_size.IsEmpty() &&
          coded_size.GetArea() <= limits::kMaxCanvas &&
          coded_size.width() <= limits::kMaxDimension &&
          coded_size.height() <= limits::kMaxDimension &&
          !visible_rect.IsEmpty() &&
          visible_rect.x() >= 0 && visible_rect.y() >= 0 &&
          visible_rect.right() <= coded_size.width() &&
          visible_rect.bottom() <= coded_size.height() &&
          !natural_size.IsEmpty() &&
          natural_size.GetArea() <= limits::kMaxCanvas &&
          natural_size.width() <= limits::kMaxDimension &&
          natural_size.height() <= limits::kMaxDimension);
}

// static
scoped_refptr<VideoFrame> VideoFrame::WrapNativeTexture(
    uint32 texture_id,
    uint32 texture_target,
    const gfx::Size& coded_size,
    const gfx::Rect& visible_rect,
    const gfx::Size& natural_size,
    base::TimeDelta timestamp,
    const ReadPixelsCB& read_pixels_cb,
    const base::Closure& no_longer_needed_cb) {
  scoped_refptr<VideoFrame> frame(
      new VideoFrame(NATIVE_TEXTURE, coded_size, visible_rect, natural_size,
                     timestamp));
  frame->texture_id_ = texture_id;
  frame->texture_target_ = texture_target;
  frame->read_pixels_cb_ = read_pixels_cb;
  frame->no_longer_needed_cb_ = no_longer_needed_cb;
  return frame;
}

void VideoFrame::ReadPixelsFromNativeTexture(void* pixels) {
  DCHECK_EQ(format_, NATIVE_TEXTURE);
  if (!read_pixels_cb_.is_null())
    read_pixels_cb_.Run(pixels);
}

// static
scoped_refptr<VideoFrame> VideoFrame::WrapExternalYuvData(
    Format format,
    const gfx::Size& coded_size,
    const gfx::Rect& visible_rect,
    const gfx::Size& natural_size,
    int32 y_stride, int32 u_stride, int32 v_stride,
    uint8* y_data, uint8* u_data, uint8* v_data,
    base::TimeDelta timestamp,
    const base::Closure& no_longer_needed_cb) {
  DCHECK(format == YV12 || format == YV16 || format == I420) << format;
  scoped_refptr<VideoFrame> frame(new VideoFrame(
      format, coded_size, visible_rect, natural_size, timestamp));
  frame->strides_[kYPlane] = y_stride;
  frame->strides_[kUPlane] = u_stride;
  frame->strides_[kVPlane] = v_stride;
  frame->data_[kYPlane] = y_data;
  frame->data_[kUPlane] = u_data;
  frame->data_[kVPlane] = v_data;
  frame->no_longer_needed_cb_ = no_longer_needed_cb;
  return frame;
}

// static
scoped_refptr<VideoFrame> VideoFrame::CreateEmptyFrame() {
  return new VideoFrame(
      VideoFrame::EMPTY, gfx::Size(), gfx::Rect(), gfx::Size(),
      base::TimeDelta());
}

// static
scoped_refptr<VideoFrame> VideoFrame::CreateColorFrame(
    const gfx::Size& size,
    uint8 y, uint8 u, uint8 v,
    base::TimeDelta timestamp) {
  DCHECK(IsValidConfig(VideoFrame::YV12, size, gfx::Rect(size), size));
  scoped_refptr<VideoFrame> frame = VideoFrame::CreateFrame(
      VideoFrame::YV12, size, gfx::Rect(size), size, timestamp);
  FillYUV(frame, y, u, v);
  return frame;
}

// static
scoped_refptr<VideoFrame> VideoFrame::CreateBlackFrame(const gfx::Size& size) {
  const uint8 kBlackY = 0x00;
  const uint8 kBlackUV = 0x80;
  const base::TimeDelta kZero;
  return CreateColorFrame(size, kBlackY, kBlackUV, kBlackUV, kZero);
}

static inline size_t RoundUp(size_t value, size_t alignment) {
  // Check that |alignment| is a power of 2.
  DCHECK((alignment + (alignment - 1)) == (alignment | (alignment - 1)));
  return ((value + (alignment - 1)) & ~(alignment-1));
}

static const int kFrameSizeAlignment = 16;
// Allows faster SIMD YUV convert. Also, FFmpeg overreads/-writes occasionally.
static const int kFramePadBytes = 15;

// Release data allocated by AllocateRGB() or AllocateYUV().
static void ReleaseData(uint8* data) {
  DCHECK(data);
  if (data) {
#if !defined(OS_ANDROID)
    av_free(data);
#else
    delete[] data;
#endif
  }
}

void VideoFrame::AllocateRGB(size_t bytes_per_pixel) {
  // Round up to align at least at a 16-byte boundary for each row.
  // This is sufficient for MMX and SSE2 reads (movq/movdqa).
  size_t bytes_per_row = RoundUp(coded_size_.width(),
                                 kFrameSizeAlignment) * bytes_per_pixel;
  size_t aligned_height = RoundUp(coded_size_.height(), kFrameSizeAlignment);
  strides_[VideoFrame::kRGBPlane] = bytes_per_row;
#if !defined(OS_ANDROID)
  // TODO(dalecurtis): use DataAligned or so, so this #ifdef hackery
  // doesn't need to be repeated in every single user of aligned data.
  data_[VideoFrame::kRGBPlane] = reinterpret_cast<uint8*>(
      av_malloc(bytes_per_row * aligned_height + kFramePadBytes));
#else
  data_[VideoFrame::kRGBPlane] = new uint8_t[bytes_per_row * aligned_height];
#endif
  no_longer_needed_cb_ = base::Bind(&ReleaseData, data_[VideoFrame::kRGBPlane]);
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
  // The *2 here is because some formats (e.g. h264) allow interlaced coding,
  // and then the size needs to be a multiple of two macroblocks (vertically).
  // See libavcodec/utils.c:avcodec_align_dimensions2().
  size_t y_height = RoundUp(coded_size_.height(), kFrameSizeAlignment * 2);
  size_t uv_height = format_ == VideoFrame::YV12 ? y_height / 2 : y_height;
  size_t y_bytes = y_height * y_stride;
  size_t uv_bytes = uv_height * uv_stride;

#if !defined(OS_ANDROID)
  // TODO(dalecurtis): use DataAligned or so, so this #ifdef hackery
  // doesn't need to be repeated in every single user of aligned data.
  // The extra line of UV being allocated is because h264 chroma MC
  // overreads by one line in some cases, see libavcodec/utils.c:
  // avcodec_align_dimensions2() and libavcodec/x86/h264_chromamc.asm:
  // put_h264_chroma_mc4_ssse3().
  uint8* data = reinterpret_cast<uint8*>(
      av_malloc(y_bytes + (uv_bytes * 2 + uv_stride) + kFramePadBytes));
#else
  uint8* data = new uint8_t[y_bytes + (uv_bytes * 2)];
#endif
  no_longer_needed_cb_ = base::Bind(&ReleaseData, data);
  COMPILE_ASSERT(0 == VideoFrame::kYPlane, y_plane_data_must_be_index_0);
  data_[VideoFrame::kYPlane] = data;
  data_[VideoFrame::kUPlane] = data + y_bytes;
  data_[VideoFrame::kVPlane] = data + y_bytes + uv_bytes;
  strides_[VideoFrame::kYPlane] = y_stride;
  strides_[VideoFrame::kUPlane] = uv_stride;
  strides_[VideoFrame::kVPlane] = uv_stride;
}

VideoFrame::VideoFrame(VideoFrame::Format format,
                       const gfx::Size& coded_size,
                       const gfx::Rect& visible_rect,
                       const gfx::Size& natural_size,
                       base::TimeDelta timestamp)
    : format_(format),
      coded_size_(coded_size),
      visible_rect_(visible_rect),
      natural_size_(natural_size),
      texture_id_(0),
      texture_target_(0),
      timestamp_(timestamp) {
  memset(&strides_, 0, sizeof(strides_));
  memset(&data_, 0, sizeof(data_));
}

VideoFrame::~VideoFrame() {
  if (!no_longer_needed_cb_.is_null())
    base::ResetAndReturn(&no_longer_needed_cb_).Run();
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
  int width = coded_size_.width();
  switch (format_) {
    // 32bpp.
    case RGB32:
      return width * 4;

    // Planar, 8bpp.
    case YV12:
    case YV16:
      if (plane == kYPlane)
        return width;
      return RoundUp(width, 2) / 2;

    default:
      break;
  }

  // Intentionally leave out non-production formats.
  NOTREACHED() << "Unsupported video frame format: " << format_;
  return 0;
}

int VideoFrame::rows(size_t plane) const {
  DCHECK(IsValidPlane(plane));
  int height = coded_size_.height();
  switch (format_) {
    case RGB32:
    case YV16:
      return height;

    case YV12:
      if (plane == kYPlane)
        return height;
      return RoundUp(height, 2) / 2;

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
  for (int plane = 0; plane < kMaxPlanes; ++plane) {
    if (!IsValidPlane(plane))
      break;
    for (int row = 0; row < rows(plane); ++row) {
      base::MD5Update(context, base::StringPiece(
          reinterpret_cast<char*>(data(plane) + stride(plane) * row),
          row_bytes(plane)));
    }
  }
}

}  // namespace media
