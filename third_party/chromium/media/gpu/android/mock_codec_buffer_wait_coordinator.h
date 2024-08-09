// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_CHROMIUM_MEDIA_GPU_ANDROID_MOCK_CODEC_BUFFER_WAIT_COORDINATOR_H_
#define THIRD_PARTY_CHROMIUM_MEDIA_GPU_ANDROID_MOCK_CODEC_BUFFER_WAIT_COORDINATOR_H_

#include "gpu/command_buffer/service/mock_texture_owner.h"
#include "third_party/chromium/media/gpu/android/codec_buffer_wait_coordinator.h"

namespace media_m96 {

// Mock class with mostly fake functions.
class MockCodecBufferWaitCoordinator : public CodecBufferWaitCoordinator {
 public:
  MockCodecBufferWaitCoordinator(
      scoped_refptr<NiceMock<gpu::MockTextureOwner>> texture_owner);

  MOCK_CONST_METHOD0(texture_owner,
                     scoped_refptr<NiceMock<gpu::MockTextureOwner>>());
  MOCK_METHOD0(SetReleaseTimeToNow, void());
  MOCK_METHOD0(IsExpectingFrameAvailable, bool());
  MOCK_METHOD0(WaitForFrameAvailable, void());

  // Fake implementations that the mocks will call by default.
  void FakeSetReleaseTimeToNow() { expecting_frame_available = true; }
  bool FakeIsExpectingFrameAvailable() { return expecting_frame_available; }
  void FakeWaitForFrameAvailable() { expecting_frame_available = false; }

  scoped_refptr<NiceMock<gpu::MockTextureOwner>> mock_texture_owner;
  bool expecting_frame_available;

 protected:
  ~MockCodecBufferWaitCoordinator();
};

}  // namespace media_m96

#endif  // THIRD_PARTY_CHROMIUM_MEDIA_GPU_ANDROID_MOCK_CODEC_BUFFER_WAIT_COORDINATOR_H_
