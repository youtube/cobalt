// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_VIDEO_FRAME_H_
#define MEDIA_BASE_VIDEO_FRAME_H_

#include "media/base/buffers.h"

namespace media {

class VideoFrame : public StreamSample {
 public:
  static const size_t kMaxPlanes = 3;

  static const size_t kNumRGBPlanes = 1;
  static const size_t kRGBPlane = 0;

  static const size_t kNumYUVPlanes = 3;
  static const size_t kYPlane = 0;
  static const size_t kUPlane = 1;
  static const size_t kVPlane = 2;

  // Surface formats roughly based on FOURCC labels, see:
  // http://www.fourcc.org/rgb.php
  // http://www.fourcc.org/yuv.php
  enum Format {
    INVALID,     // Invalid format value.  Used for error reporting.
    RGB555,      // 16bpp RGB packed 5:5:5
    RGB565,      // 16bpp RGB packed 5:6:5
    RGB24,       // 24bpp RGB packed 8:8:8
    RGB32,       // 32bpp RGB packed with extra byte 8:8:8
    RGBA,        // 32bpp RGBA packed 8:8:8:8
    YV12,        // 12bpp YVU planar 1x1 Y, 2x2 VU samples
    YV16,        // 16bpp YVU planar 1x1 Y, 2x1 VU samples
    EMPTY,       // An empty frame.
  };

  enum BufferType {
    TYPE_SYSTEM_MEMORY,
    TYPE_OMX_BUFFER_HEAD,
    TYPE_EGL_IMAGE,
  };

 public:
  // Creates a new frame with given parameters. Buffers for the frame are
  // allocated but not initialized.
  static void CreateFrame(Format format,
                          size_t width,
                          size_t height,
                          base::TimeDelta timestamp,
                          base::TimeDelta duration,
                          scoped_refptr<VideoFrame>* frame_out);

  // Creates a frame with format equals to VideoFrame::EMPTY, width, height
  // timestamp and duration are all 0.
  static void CreateEmptyFrame(scoped_refptr<VideoFrame>* frame_out);

  // Allocates YV12 frame based on |width| and |height|, and sets its data to
  // the YUV equivalent of RGB(0,0,0).
  static void CreateBlackFrame(int width, int height,
                               scoped_refptr<VideoFrame>* frame_out);

  virtual BufferType type() const { return TYPE_SYSTEM_MEMORY; }

  Format format() const { return format_; }

  size_t width() const { return width_; }

  size_t height() const { return height_; }

  size_t planes() const { return planes_;  }

  int32 stride(size_t plane) const { return strides_[plane]; }

  // Returns pointer to the buffer for a given plane. The memory is owned by
  // VideoFrame object and must not be freed by the caller.
  uint8* data(size_t plane) const { return data_[plane]; }

  // StreamSample interface.
  virtual bool IsEndOfStream() const;

 protected:
  // Clients must use the static CreateFrame() method to create a new frame.
  VideoFrame(Format format,
             size_t video_width,
             size_t video_height);

  virtual ~VideoFrame();

  // Used internally by CreateFrame().
  bool AllocateRGB(size_t bytes_per_pixel);
  bool AllocateYUV();

  // Frame format.
  Format format_;

  // Width and height of surface.
  size_t width_;
  size_t height_;

  // Number of planes, typically 1 for packed RGB formats and 3 for planar
  // YUV formats.
  size_t planes_;

  // Array of strides for each plane, typically greater or equal to the width
  // of the surface divided by the horizontal sampling period.  Note that
  // strides can be negative.
  int32 strides_[kMaxPlanes];

  // Array of data pointers to each plane.
  uint8* data_[kMaxPlanes];

  DISALLOW_COPY_AND_ASSIGN(VideoFrame);
};

}  // namespace media

#endif  // MEDIA_BASE_VIDEO_FRAME_H_
