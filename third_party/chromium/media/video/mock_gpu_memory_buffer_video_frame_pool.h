// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_CHROMIUM_MEDIA_VIDEO_MOCK_GPU_MEMORY_BUFFER_VIDEO_FRAME_POOL_H_
#define THIRD_PARTY_CHROMIUM_MEDIA_VIDEO_MOCK_GPU_MEMORY_BUFFER_VIDEO_FRAME_POOL_H_

#include "base/callback.h"
#include "third_party/chromium/media/video/gpu_memory_buffer_video_frame_pool.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace media_m96 {

class MockGpuMemoryBufferVideoFramePool : public GpuMemoryBufferVideoFramePool {
 public:
  explicit MockGpuMemoryBufferVideoFramePool(
      std::vector<base::OnceClosure>* frame_ready_cbs);
  ~MockGpuMemoryBufferVideoFramePool() override;

  void MaybeCreateHardwareFrame(scoped_refptr<VideoFrame> video_frame,
                                FrameReadyCB frame_ready_cb) override;
  MOCK_METHOD0(Abort, void());

 private:
  std::vector<base::OnceClosure>* frame_ready_cbs_;
};

}  // namespace media_m96

#endif  // THIRD_PARTY_CHROMIUM_MEDIA_VIDEO_MOCK_GPU_MEMORY_BUFFER_VIDEO_FRAME_POOL_H_
