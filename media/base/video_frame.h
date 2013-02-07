// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_VIDEO_FRAME_H_
#define MEDIA_BASE_VIDEO_FRAME_H_

#include "base/callback.h"
#include "base/md5.h"
#include "media/base/buffers.h"
#include "ui/gfx/rect.h"
#include "ui/gfx/size.h"

namespace media {

class MEDIA_EXPORT VideoFrame : public base::RefCountedThreadSafe<VideoFrame> {
 public:
  enum {
    kFrameSizeAlignment = 16,
    kFrameSizePadding = 16,
    kFrameAddressAlignment = 32
  };

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
  // |coded_size| is the width and height of the frame data in pixels.
  // |visible_rect| is the visible portion of |coded_size|, after cropping (if
  // any) is applied.
  // |natural_size| is the width and height of the frame when the frame's aspect
  // ratio is applied to |visible_rect|.
  static scoped_refptr<VideoFrame> CreateFrame(
      Format format,
      const gfx::Size& coded_size,
      const gfx::Rect& visible_rect,
      const gfx::Size& natural_size,
      base::TimeDelta timestamp);

  // Call prior to CreateFrame to ensure validity of frame configuration. Called
  // automatically by VideoDecoderConfig::IsValidConfig().
  // TODO(scherkus): VideoDecoderConfig shouldn't call this method
  static bool IsValidConfig(Format format, const gfx::Size& coded_size,
                            const gfx::Rect& visible_rect,
                            const gfx::Size& natural_size);

  // CB to write pixels from the texture backing this frame into the
  // |void*| parameter.
  typedef base::Callback<void(void*)> ReadPixelsCB;

  // Wraps a native texture of the given parameters with a VideoFrame.  When the
  // frame is destroyed |no_longer_needed_cb.Run()| will be called.
  // |coded_size| is the width and height of the frame data in pixels.
  // |visible_rect| is the visible portion of |coded_size|, after cropping (if
  // any) is applied.
  // |natural_size| is the width and height of the frame when the frame's aspect
  // ratio is applied to |visible_rect|.
  // |read_pixels_cb| may be used to do (slow!) readbacks from the
  // texture to main memory.
  static scoped_refptr<VideoFrame> WrapNativeTexture(
      uint32 texture_id,
      uint32 texture_target,
      const gfx::Size& coded_size,
      const gfx::Rect& visible_rect,
      const gfx::Size& natural_size,
      base::TimeDelta timestamp,
      const ReadPixelsCB& read_pixels_cb,
      const base::Closure& no_longer_needed_cb);

  // Read pixels from the native texture backing |*this| and write
  // them to |*pixels| as BGRA.  |pixels| must point to a buffer at
  // least as large as 4*visible_rect().width()*visible_rect().height().
  void ReadPixelsFromNativeTexture(void* pixels);

  // Wraps external YUV data of the given parameters with a VideoFrame.
  // The returned VideoFrame does not own the data passed in. When the frame
  // is destroyed |no_longer_needed_cb.Run()| will be called.
  static scoped_refptr<VideoFrame> WrapExternalYuvData(
      Format format,
      const gfx::Size& coded_size,
      const gfx::Rect& visible_rect,
      const gfx::Size& natural_size,
      int32 y_stride,
      int32 u_stride,
      int32 v_stride,
      uint8* y_data,
      uint8* u_data,
      uint8* v_data,
      base::TimeDelta timestamp,
      const base::Closure& no_longer_needed_cb);

  // Creates a frame with format equals to VideoFrame::EMPTY, width, height,
  // and timestamp are all 0.
  static scoped_refptr<VideoFrame> CreateEmptyFrame();

  // Allocates YV12 frame based on |size|, and sets its data to the YUV(y,u,v).
  static scoped_refptr<VideoFrame> CreateColorFrame(
      const gfx::Size& size,
      uint8 y, uint8 u, uint8 v,
      base::TimeDelta timestamp);

  // Allocates YV12 frame based on |size|, and sets its data to the YUV
  // equivalent of RGB(0,0,0).
  static scoped_refptr<VideoFrame> CreateBlackFrame(const gfx::Size& size);

  Format format() const { return format_; }

  const gfx::Size& coded_size() const { return coded_size_; }
  const gfx::Rect& visible_rect() const { return visible_rect_; }
  const gfx::Size& natural_size() const { return natural_size_; }

  int stride(size_t plane) const;

  // Returns the number of bytes per row and number of rows for a given plane.
  //
  // As opposed to stride(), row_bytes() refers to the bytes representing
  // frame data scanlines (coded_size.width() pixels, without stride padding).
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

  // Used to keep a running hash of seen frames.  Expects an initialized MD5
  // context.  Calls MD5Update with the context and the contents of the frame.
  void HashFrameForTesting(base::MD5Context* context);

 private:
  friend class base::RefCountedThreadSafe<VideoFrame>;
  // Clients must use the static CreateFrame() method to create a new frame.
  VideoFrame(Format format,
             const gfx::Size& coded_size,
             const gfx::Rect& visible_rect,
             const gfx::Size& natural_size,
             base::TimeDelta timestamp);
  virtual ~VideoFrame();

  // Used internally by CreateFrame().
  void AllocateRGB(size_t bytes_per_pixel);
  void AllocateYUV();

  // Used to DCHECK() plane parameters.
  bool IsValidPlane(size_t plane) const;

  // Frame format.
  Format format_;

  // Width and height of the video frame.
  gfx::Size coded_size_;

  // Width, height, and offsets of the visible portion of the video frame.
  gfx::Rect visible_rect_;

  // Width and height of the visible portion of the video frame with aspect
  // ratio taken into account.
  gfx::Size natural_size_;

  // Array of strides for each plane, typically greater or equal to the width
  // of the surface divided by the horizontal sampling period.  Note that
  // strides can be negative.
  int32 strides_[kMaxPlanes];

  // Array of data pointers to each plane.
  uint8* data_[kMaxPlanes];

  // Native texture ID, if this is a NATIVE_TEXTURE frame.
  uint32 texture_id_;
  uint32 texture_target_;
  ReadPixelsCB read_pixels_cb_;

  base::Closure no_longer_needed_cb_;

  base::TimeDelta timestamp_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(VideoFrame);
};

}  // namespace media

#endif  // MEDIA_BASE_VIDEO_FRAME_H_
