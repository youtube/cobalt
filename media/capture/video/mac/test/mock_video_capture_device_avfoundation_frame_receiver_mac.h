// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CAPTURE_VIDEO_MAC_TEST_MOCK_VIDEO_CAPTURE_DEVICE_AVFOUNDATION_FRAME_RECEIVER_MAC_H_
#define MEDIA_CAPTURE_VIDEO_MAC_TEST_MOCK_VIDEO_CAPTURE_DEVICE_AVFOUNDATION_FRAME_RECEIVER_MAC_H_

#include "media/capture/video/mac/video_capture_device_avfoundation_mac.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace media {

class MockVideoCaptureDeviceAVFoundationFrameReceiver
    : public VideoCaptureDeviceAVFoundationFrameReceiver {
 public:
  MockVideoCaptureDeviceAVFoundationFrameReceiver();
  ~MockVideoCaptureDeviceAVFoundationFrameReceiver() override;

  MOCK_METHOD(void,
              ReceiveFrame,
              (const uint8_t* video_frame,
               int video_frame_length,
               const VideoCaptureFormat& frame_format,
               const gfx::ColorSpace color_space,
               int aspect_numerator,
               int aspect_denominator,
               base::TimeDelta timestamp),
              (override));

  MOCK_METHOD(void,
              ReceiveExternalGpuMemoryBufferFrame,
              (CapturedExternalVideoBuffer frame,
               std::vector<CapturedExternalVideoBuffer> scaled_frames,
               base::TimeDelta timestamp),
              (override));

  MOCK_METHOD(void,
              OnPhotoTaken,
              (const uint8_t* image_data,
               size_t image_length,
               const std::string& mime_type),
              (override));

  MOCK_METHOD(void, OnPhotoError, (), (override));

  MOCK_METHOD(void,
              ReceiveError,
              (VideoCaptureError error,
               const base::Location& from_here,
               const std::string& reason),
              (override));

  MOCK_METHOD(void, ReceiveCaptureConfigurationChanged, (), (override));
};

}  // namespace media

#endif  // MEDIA_CAPTURE_VIDEO_MAC_TEST_MOCK_VIDEO_CAPTURE_DEVICE_AVFOUNDATION_FRAME_RECEIVER_MAC_H_
