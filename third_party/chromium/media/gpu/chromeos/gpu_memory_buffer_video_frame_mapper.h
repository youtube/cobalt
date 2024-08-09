// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_CHROMIUM_MEDIA_GPU_CHROMEOS_GPU_MEMORY_BUFFER_VIDEO_FRAME_MAPPER_H_
#define THIRD_PARTY_CHROMIUM_MEDIA_GPU_CHROMEOS_GPU_MEMORY_BUFFER_VIDEO_FRAME_MAPPER_H_

#include "third_party/chromium/media/gpu/media_gpu_export.h"
#include "third_party/chromium/media/gpu/video_frame_mapper.h"

namespace media_m96 {

// The GpuMemoryBufferVideoFrameMapper implements functionality to map
// GpuMemoryBuffer-based video frames into memory.
class MEDIA_GPU_EXPORT GpuMemoryBufferVideoFrameMapper
    : public VideoFrameMapper {
 public:
  static std::unique_ptr<GpuMemoryBufferVideoFrameMapper> Create(
      VideoPixelFormat format);

  GpuMemoryBufferVideoFrameMapper(const GpuMemoryBufferVideoFrameMapper&) =
      delete;
  GpuMemoryBufferVideoFrameMapper& operator=(
      const GpuMemoryBufferVideoFrameMapper&) = delete;

  ~GpuMemoryBufferVideoFrameMapper() override = default;

  // VideoFrameMapper implementation.
  scoped_refptr<VideoFrame> Map(
      scoped_refptr<const VideoFrame> video_frame) const override;

 private:
  explicit GpuMemoryBufferVideoFrameMapper(VideoPixelFormat format);
};

}  // namespace media_m96
#endif  // THIRD_PARTY_CHROMIUM_MEDIA_GPU_CHROMEOS_GPU_MEMORY_BUFFER_VIDEO_FRAME_MAPPER_H_
