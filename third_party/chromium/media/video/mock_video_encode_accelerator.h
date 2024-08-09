// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_CHROMIUM_MEDIA_VIDEO_MOCK_VIDEO_ENCODE_ACCELERATOR_H_
#define THIRD_PARTY_CHROMIUM_MEDIA_VIDEO_MOCK_VIDEO_ENCODE_ACCELERATOR_H_

#include "third_party/chromium/media/video/video_encode_accelerator.h"

#include "base/macros.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace media_m96 {

class MockVideoEncodeAccelerator : public VideoEncodeAccelerator {
 public:
  MockVideoEncodeAccelerator();

  MockVideoEncodeAccelerator(const MockVideoEncodeAccelerator&) = delete;
  MockVideoEncodeAccelerator& operator=(const MockVideoEncodeAccelerator&) =
      delete;

  ~MockVideoEncodeAccelerator() override;

  MOCK_METHOD0(GetSupportedProfiles,
               VideoEncodeAccelerator::SupportedProfiles());
  MOCK_METHOD2(Initialize,
               bool(const VideoEncodeAccelerator::Config& config,
                    VideoEncodeAccelerator::Client* client));
  MOCK_METHOD2(Encode,
               void(scoped_refptr<VideoFrame> frame, bool force_keyframe));
  MOCK_METHOD1(UseOutputBitstreamBuffer, void(BitstreamBuffer buffer));
  MOCK_METHOD2(RequestEncodingParametersChange,
               void(const Bitrate& bitrate, uint32_t framerate));
  MOCK_METHOD0(Destroy, void());

 private:
  void DeleteThis();
};

}  // namespace media_m96

#endif  // THIRD_PARTY_CHROMIUM_MEDIA_VIDEO_MOCK_VIDEO_ENCODE_ACCELERATOR_H_
