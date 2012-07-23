// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_VIDEO_FRAME_H_
#define MEDIA_BASE_VIDEO_FRAME_H_

#include "base/callback.h"
#include "base/md5.h"
#include "media/base/buffers.h"

namespace media {

class MEDIA_EXPORT VideoFrame : public base::RefCountedThreadSafe<VideoFrame> {
 public:
  enum {
    kMaxPlanes = 3,

    kRGBPlane = 0,

    kYPlane = 0,
    kUPlane = 1,
    kVPlane = 2,
  };

  // Surface formats roughly based on FOURCC labels, see:
  // http://www.fourcc.org/rgb.php
  // http://www.fourcc.org/yuv.php
  // Keep in sync with WebKit::WebVideoFrame!
  enum Format {
    INVALID = 0,  // Invalid format value.  Used for error reporting.
    RGB32 = 4,  // 32bpp RGB packed with extra byte 8:8:8
    YV12 = 6,  // 12bpp YVU planar 1x1 Y, 2x2 VU samples
    YV16 = 7,  // 16bpp YVU planar 1x1 Y, 2x1 VU samples
    EMPTY = 9,  // An empty frame.
    I420 = 11,  // 12bpp YVU planar 1x1 Y, 2x2 UV samples.
    NATIVE_TEXTURE = 12,  // Native texture.  Pixel-format agnostic.
  };

  // Creates a new frame in system memory with given parameters. Buffers for
  // the frame are allocated but not initialized.
  static scoped_refptr<VideoFrame> CreateFrame(
      Format format,
      size_t width,
      size_t height,
      base::TimeDelta timestamp,
      base::TimeDelta duration);

  // Call prior to CreateFrame to ensure validity of frame configuration. Called
  // automatically by VideoDecoderConfig::IsValidConfig().
  // TODO(scherkus): VideoDecoderConfig shouldn't call this method
  static bool IsValidConfig(
      Format format,
      size_t width,
      size_t height);

  // Wraps a native texture of the given parameters with a VideoFrame.  When the
  // frame is destroyed |no_longer_needed.Run()| will be called.
  static scoped_refptr<VideoFrame> WrapNativeTexture(
      uint32 texture_id,
      uint32 texture_target,
      size_t width,
      size_t height,
      base::TimeDelta timestamp,
      base::TimeDelta duration,
      const base::Closure& no_longer_needed);

  // Creates a frame with format equals to VideoFrame::EMPTY, width, height
  // timestamp and duration are all 0.
  static scoped_refptr<VideoFrame> CreateEmptyFrame();

  // Allocates YV12 frame based on |width| and |height|, and sets its data to
  // the YUV equivalent of RGB(0,0,0).
  static scoped_refptr<VideoFrame> CreateBlackFrame(int width, int height);

  Format format() const { return format_; }

  size_t width() const { return width_; }

  size_t height() const { return height_; }

  int stride(size_t plane) const;

  // Returns the number of bytes per row and number of rows for a given plane.
  //
  // As opposed to stride(), row_bytes() refers to the bytes representing
  // visible pixels.
  int row_bytes(size_t plane) const;
  int rows(size_t plane) const;

  // Returns pointer to the buffer for a given plane. The memory is owned by
  // VideoFrame object and must not be freed by the caller.
  uint8* data(size_t plane) const;

  // Returns the ID of the native texture wrapped by this frame.  Only valid to
  // call if this is a NATIVE_TEXTURE frame.
  uint32 texture_id() const;

  // Returns the texture target. Only valid for NATIVE_TEXTURE frames.
  uint32 texture_target() const;

  // Returns true if this VideoFrame represents the end of the stream.
  bool IsEndOfStream() const;

  base::TimeDelta GetTimestamp() const {
    return timestamp_;
  }
  void SetTimestamp(const base::TimeDelta& timestamp) {
    timestamp_ = timestamp;
  }

  base::TimeDelta GetDuration() const {
    return duration_;
  }
  void SetDuration(const base::TimeDelta& duration) {
    duration_ = duration;
  }

  // Used to keep a running hash of seen frames.  Expects an initialized MD5
  // context.  Calls MD5Update with the context and the contents of the frame.
  void HashFrameForTesting(base::MD5Context* context);

 private:
  friend class base::RefCountedThreadSafe<VideoFrame>;
  // Clients must use the static CreateFrame() method to create a new frame.
  VideoFrame(Format format,
             size_t video_width,
             size_t video_height,
             base::TimeDelta timestamp,
             base::TimeDelta duration);
  virtual ~VideoFrame();

  // Used internally by CreateFrame().
  void AllocateRGB(size_t bytes_per_pixel);
  void AllocateYUV();

  // Used to DCHECK() plane parameters.
  bool IsValidPlane(size_t plane) const;

  // Frame format.
  Format format_;

  // Width and height of surface.
  size_t width_;
  size_t height_;

  // Array of strides for each plane, typically greater or equal to the width
  // of the surface divided by the horizontal sampling period.  Note that
  // strides can be negative.
  int32 strides_[kMaxPlanes];

  // Array of data pointers to each plane.
  uint8* data_[kMaxPlanes];

  // Native texture ID, if this is a NATIVE_TEXTURE frame.
  uint32 texture_id_;
  uint32 texture_target_;
  base::Closure texture_no_longer_needed_;

  base::TimeDelta timestamp_;
  base::TimeDelta duration_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(VideoFrame);
};

}  // namespace media

#endif  // MEDIA_BASE_VIDEO_FRAME_H_
