// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/chromium/media/video/mock_gpu_memory_buffer_video_frame_pool.h"

#include "base/bind.h"

namespace media_m96 {

MockGpuMemoryBufferVideoFramePool::MockGpuMemoryBufferVideoFramePool(
    std::vector<base::OnceClosure>* frame_ready_cbs)
    : frame_ready_cbs_(frame_ready_cbs) {}

MockGpuMemoryBufferVideoFramePool::~MockGpuMemoryBufferVideoFramePool() {}

void MockGpuMemoryBufferVideoFramePool::MaybeCreateHardwareFrame(
    scoped_refptr<VideoFrame> video_frame,
    FrameReadyCB frame_ready_cb) {
  frame_ready_cbs_->push_back(
      base::BindOnce(std::move(frame_ready_cb), std::move(video_frame)));
}

}  // namespace media_m96
