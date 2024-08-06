// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_CHROMIUM_MEDIA_GPU_VIDEO_FRAME_MAPPER_H_
#define THIRD_PARTY_CHROMIUM_MEDIA_GPU_VIDEO_FRAME_MAPPER_H_

#include "base/macros.h"
#include "base/memory/scoped_refptr.h"
#include "third_party/chromium/media/base/video_frame.h"
#include "third_party/chromium/media/base/video_types.h"
#include "third_party/chromium/media/gpu/media_gpu_export.h"

namespace media_m96 {

// The VideoFrameMapper interface allows mapping video frames into memory so
// that their contents can be accessed directly.
// VideoFrameMapper should be created by using VideoFrameMapperFactory.
class MEDIA_GPU_EXPORT VideoFrameMapper {
 public:
  VideoFrameMapper(const VideoFrameMapper&) = delete;
  VideoFrameMapper& operator=(const VideoFrameMapper&) = delete;

  virtual ~VideoFrameMapper() = default;

  // Maps data referred by |video_frame| and creates a VideoFrame whose dtor
  // unmap the mapped memory.
  virtual scoped_refptr<VideoFrame> Map(
      scoped_refptr<const VideoFrame> video_frame) const = 0;

  // Returns the allowed pixel format of video frames on Map().
  VideoPixelFormat pixel_format() const { return format_; }

 protected:
  explicit VideoFrameMapper(VideoPixelFormat format) : format_(format) {}

  // The allowed pixel format of video frames on Map().
  VideoPixelFormat format_;
};

}  // namespace media_m96

#endif  // THIRD_PARTY_CHROMIUM_MEDIA_GPU_VIDEO_FRAME_MAPPER_H_
