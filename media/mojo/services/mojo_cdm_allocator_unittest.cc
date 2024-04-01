// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>

#include <cstring>

#include "base/macros.h"
#include "media/base/video_frame.h"
#include "media/cdm/api/content_decryption_module.h"
#include "media/cdm/cdm_helpers.h"
#include "media/mojo/services/mojo_cdm_allocator.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/size.h"

namespace media {

class MojoCdmAllocatorTest : public testing::Test {
 public:
  MojoCdmAllocatorTest() = default;

  MojoCdmAllocatorTest(const MojoCdmAllocatorTest&) = delete;
  MojoCdmAllocatorTest& operator=(const MojoCdmAllocatorTest&) = delete;

  ~MojoCdmAllocatorTest() override = default;

 protected:
  cdm::Buffer* CreateCdmBuffer(size_t capacity) {
    return allocator_.CreateCdmBuffer(capacity);
  }

  std::unique_ptr<VideoFrameImpl> CreateCdmVideoFrame() {
    return allocator_.CreateCdmVideoFrame();
  }

  MojoHandle GetHandle(cdm::Buffer* buffer) {
    return allocator_.GetHandleForTesting(buffer);
  }

  size_t GetAvailableBufferCount() {
    return allocator_.GetAvailableBufferCountForTesting();
  }

 private:
  MojoCdmAllocator allocator_;
};

TEST_F(MojoCdmAllocatorTest, CreateCdmBuffer) {
  cdm::Buffer* buffer = CreateCdmBuffer(100);
  EXPECT_GE(buffer->Capacity(), 100u);
  buffer->SetSize(50);
  EXPECT_EQ(50u, buffer->Size());
  buffer->Destroy();
}

TEST_F(MojoCdmAllocatorTest, ReuseCdmBuffer) {
  const size_t kRandomDataSize = 46;

  // Create a small buffer.
  cdm::Buffer* buffer = CreateCdmBuffer(kRandomDataSize);
  MojoHandle handle = GetHandle(buffer);
  buffer->Destroy();

  // Now allocate a new buffer of the same size, it should reuse the one
  // just freed.
  cdm::Buffer* new_buffer = CreateCdmBuffer(kRandomDataSize);
  EXPECT_EQ(handle, GetHandle(new_buffer));
  new_buffer->Destroy();
}

TEST_F(MojoCdmAllocatorTest, MaxFreeBuffers) {
  const size_t kMaxExpectedFreeBuffers = 3;
  size_t buffer_size = 0;
  const size_t kBufferSizeIncrease = 1000;
  std::vector<cdm::Buffer*> buffers;

  // Allocate and destroy 10 buffers in increasing size (to avoid buffer reuse).
  // Eventually allocating a new buffer will free the smallest free buffer, so
  // the number of free buffers will be capped at |kMaxExpectedFreeBuffers|.
  for (int i = 0; i < 10; ++i) {
    buffer_size += kBufferSizeIncrease;
    cdm::Buffer* buffer = CreateCdmBuffer(buffer_size);
    buffer->Destroy();
    EXPECT_LE(GetAvailableBufferCount(), kMaxExpectedFreeBuffers);
  }
}

TEST_F(MojoCdmAllocatorTest, CreateCdmVideoFrame) {
  const int kWidth = 16;
  const int kHeight = 9;
  const VideoPixelFormat kFormat = PIXEL_FORMAT_I420;
  const gfx::Size kSize(kWidth, kHeight);
  const size_t kBufferSize = VideoFrame::AllocationSize(kFormat, kSize);

  // Create a VideoFrameImpl and initialize it.
  std::unique_ptr<VideoFrameImpl> video_frame = CreateCdmVideoFrame();
  video_frame->SetFormat(cdm::kI420);
  video_frame->SetSize({kWidth, kHeight});
  video_frame->SetStride(
      cdm::kYPlane, static_cast<uint32_t>(
                        VideoFrame::RowBytes(cdm::kYPlane, kFormat, kWidth)));
  video_frame->SetStride(
      cdm::kUPlane, static_cast<uint32_t>(
                        VideoFrame::RowBytes(cdm::kUPlane, kFormat, kWidth)));
  video_frame->SetStride(
      cdm::kVPlane, static_cast<uint32_t>(
                        VideoFrame::RowBytes(cdm::kVPlane, kFormat, kWidth)));
  EXPECT_EQ(nullptr, video_frame->FrameBuffer());

  // Now create a buffer to hold the frame and assign it to the VideoFrameImpl.
  cdm::Buffer* buffer = CreateCdmBuffer(kBufferSize);
  EXPECT_EQ(0u, GetAvailableBufferCount());
  buffer->SetSize(static_cast<uint32_t>(kBufferSize));
  video_frame->SetFrameBuffer(buffer);
  EXPECT_NE(nullptr, video_frame->FrameBuffer());

  // Transform it into a VideoFrame and make sure the buffer is no longer owned.
  scoped_refptr<VideoFrame> frame = video_frame->TransformToVideoFrame(kSize);
  EXPECT_EQ(nullptr, video_frame->FrameBuffer());
  EXPECT_EQ(0u, GetAvailableBufferCount());
  video_frame.reset();

  // Check that the buffer is still in use. It will be freed when |frame|
  // is destroyed.
  EXPECT_EQ(0u, GetAvailableBufferCount());
  frame = nullptr;

  // Check that the buffer is now in the free list.
  EXPECT_EQ(1u, GetAvailableBufferCount());
}

}  // namespace media
