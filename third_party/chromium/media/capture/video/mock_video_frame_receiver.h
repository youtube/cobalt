// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CAPTURE_VIDEO_MOCK_VIDEO_FRAME_RECEIVER_H_
#define MEDIA_CAPTURE_VIDEO_MOCK_VIDEO_FRAME_RECEIVER_H_

#include "media/capture/video/video_frame_receiver.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace media {

class MockVideoFrameReceiver : public VideoFrameReceiver {
 public:
  MockVideoFrameReceiver();
  ~MockVideoFrameReceiver() override;

  MOCK_METHOD1(MockOnNewBufferHandle, void(int buffer_id));
  MOCK_METHOD3(
      MockOnFrameReadyInBuffer,
      void(int buffer_id,
           std::unique_ptr<
               VideoCaptureDevice::Client::Buffer::ScopedAccessPermission>*
               buffer_read_permission,
           const gfx::Size&));
  MOCK_METHOD1(OnError, void(media::VideoCaptureError error));
  MOCK_METHOD1(OnFrameDropped, void(media::VideoCaptureFrameDropReason reason));
  MOCK_METHOD1(OnLog, void(const std::string& message));
  MOCK_METHOD1(OnBufferRetired, void(int buffer_id));
  MOCK_METHOD0(OnStarted, void());
  MOCK_METHOD0(OnStartedUsingGpuDecode, void());
  MOCK_METHOD0(OnStopped, void());

  void OnNewBuffer(int buffer_id,
                   media::mojom::VideoBufferHandlePtr buffer_handle) override {
    MockOnNewBufferHandle(buffer_id);
  }

  void OnFrameReadyInBuffer(
      ReadyFrameInBuffer frame,
      std::vector<ReadyFrameInBuffer> scaled_frames) override {
    MockOnFrameReadyInBuffer(frame.buffer_id, &frame.buffer_read_permission,
                             frame.frame_info->coded_size);
  }
};

}  // namespace media

#endif  // MEDIA_CAPTURE_VIDEO_MOCK_VIDEO_FRAME_RECEIVER_H_
