// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/capture/video/mock_video_capture_device_client.h"

#include <utility>

#include "base/containers/heap_array.h"
#include "base/containers/span.h"
#include "base/memory/raw_span.h"
#include "base/time/time.h"
#include "media/base/video_frame.h"

using testing::_;
using testing::Invoke;
using testing::WithArgs;

namespace media {

namespace {

class StubBufferHandle : public VideoCaptureBufferHandle {
 public:
  explicit StubBufferHandle(base::span<uint8_t> data) : data_(data) {}

  size_t mapped_size() const override { return data_.size(); }
  uint8_t* data() const override { return data_.data(); }
  const uint8_t* const_data() const override { return data_.data(); }

 private:
  const base::raw_span<uint8_t> data_;
};

class StubBufferHandleProvider
    : public VideoCaptureDevice::Client::Buffer::HandleProvider {
 public:
  explicit StubBufferHandleProvider(base::HeapArray<uint8_t> data)
      : data_(std::move(data)) {}

  ~StubBufferHandleProvider() override = default;

  base::UnsafeSharedMemoryRegion DuplicateAsUnsafeRegion() override {
    return base::UnsafeSharedMemoryRegion();
  }

  std::unique_ptr<VideoCaptureBufferHandle> GetHandleForInProcessAccess()
      override {
    return std::make_unique<StubBufferHandle>(data_);
  }

  gfx::GpuMemoryBufferHandle GetGpuMemoryBufferHandle() override {
    return gfx::GpuMemoryBufferHandle();
  }

 private:
  base::HeapArray<uint8_t> data_;
};

class StubReadWritePermission
    : public VideoCaptureDevice::Client::Buffer::ScopedAccessPermission {
 public:
  explicit StubReadWritePermission(base::span<uint8_t> data) : data_(data) {}
  ~StubReadWritePermission() override = default;

 private:
  const base::raw_span<uint8_t> data_;
};

VideoCaptureDevice::Client::Buffer CreateStubBuffer(int buffer_id,
                                                    size_t mapped_size) {
  const int arbitrary_frame_feedback_id = 0;
  auto buffer = base::HeapArray<uint8_t>::WithSize(mapped_size);
  auto unowned_buffer = buffer.as_span();
  return VideoCaptureDevice::Client::Buffer(
      buffer_id, arbitrary_frame_feedback_id,
      std::make_unique<StubBufferHandleProvider>(std::move(buffer)),
      std::make_unique<StubReadWritePermission>(unowned_buffer));
}

}  // namespace

MockVideoCaptureDeviceClient::MockVideoCaptureDeviceClient() = default;
MockVideoCaptureDeviceClient::~MockVideoCaptureDeviceClient() = default;

void MockVideoCaptureDeviceClient::OnIncomingCapturedBuffer(
    Buffer buffer,
    const media::VideoCaptureFormat& format,
    base::TimeTicks reference_time,
    base::TimeDelta timestamp,
    std::optional<base::TimeTicks> capture_begin_time,
    const std::optional<VideoFrameMetadata>& metadata) {
  DoOnIncomingCapturedBuffer(buffer, format, reference_time, timestamp);
}
void MockVideoCaptureDeviceClient::OnIncomingCapturedBufferExt(
    Buffer buffer,
    const media::VideoCaptureFormat& format,
    const gfx::ColorSpace& color_space,
    base::TimeTicks reference_time,
    base::TimeDelta timestamp,
    std::optional<base::TimeTicks> capture_begin_time,
    gfx::Rect visible_rect,
    const std::optional<VideoFrameMetadata>& additional_metadata) {
  DoOnIncomingCapturedBufferExt(buffer, format, color_space, reference_time,
                                timestamp, visible_rect, additional_metadata);
}

// static
std::unique_ptr<MockVideoCaptureDeviceClient>
MockVideoCaptureDeviceClient::CreateMockClientWithBufferAllocator(
    FakeFrameCapturedCallback frame_captured_callback) {
  auto result = std::make_unique<NiceMockVideoCaptureDeviceClient>();
  result->fake_frame_captured_callback_ = std::move(frame_captured_callback);

  auto* raw_result_ptr = result.get();
  ON_CALL(*result, ReserveOutputBuffer)
      .WillByDefault(
          Invoke([](const gfx::Size& dimensions, VideoPixelFormat format, int,
                    VideoCaptureDevice::Client::Buffer* buffer,
                    int* require_new_buffer_id, int* retire_old_buffer_id) {
            EXPECT_GT(dimensions.GetArea(), 0);
            const VideoCaptureFormat frame_format(dimensions, 0.0, format);
            *buffer = CreateStubBuffer(
                0, VideoFrame::AllocationSize(frame_format.pixel_format,
                                              frame_format.frame_size));
            return VideoCaptureDevice::Client::ReserveResult::kSucceeded;
          }));
  ON_CALL(*result, OnIncomingCapturedData)
      .WillByDefault(WithArgs<2>(Invoke(
          [raw_result_ptr](const media::VideoCaptureFormat& frame_format) {
            raw_result_ptr->fake_frame_captured_callback_.Run(frame_format);
          })));
  ON_CALL(*result, OnIncomingCapturedImage)
      .WillByDefault(WithArgs<1>(Invoke(
          [raw_result_ptr](const media::VideoCaptureFormat& frame_format) {
            raw_result_ptr->fake_frame_captured_callback_.Run(frame_format);
          })));
  ON_CALL(*result, DoOnIncomingCapturedBuffer)
      .WillByDefault(WithArgs<1>(Invoke(
          [raw_result_ptr](const media::VideoCaptureFormat& frame_format) {
            raw_result_ptr->fake_frame_captured_callback_.Run(frame_format);
          })));
  ON_CALL(*result, DoOnIncomingCapturedBufferExt)
      .WillByDefault(WithArgs<1>(Invoke(
          [raw_result_ptr](const media::VideoCaptureFormat& frame_format) {
            raw_result_ptr->fake_frame_captured_callback_.Run(frame_format);
          })));
  return result;
}

}  // namespace media
