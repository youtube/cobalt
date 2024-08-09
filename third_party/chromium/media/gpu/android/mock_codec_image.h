// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_CHROMIUM_MEDIA_GPU_ANDROID_MOCK_CODEC_IMAGE_H_
#define THIRD_PARTY_CHROMIUM_MEDIA_GPU_ANDROID_MOCK_CODEC_IMAGE_H_

#include "base/macros.h"
#include "third_party/chromium/media/gpu/android/codec_image.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media_m96 {

// CodecImage with a mocked ReleaseCodecBuffer.
class MockCodecImage : public CodecImage {
 public:
  MockCodecImage(const gfx::Size& coded_size);

  MOCK_METHOD0(ReleaseCodecBuffer, void());

 protected:
  ~MockCodecImage() override;

  DISALLOW_COPY_AND_ASSIGN(MockCodecImage);
};

}  // namespace media_m96

#endif  // THIRD_PARTY_CHROMIUM_MEDIA_GPU_ANDROID_MOCK_CODEC_IMAGE_H_
