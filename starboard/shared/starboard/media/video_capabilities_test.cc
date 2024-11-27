// Copyright 2020 The Cobalt Authors. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "starboard/shared/starboard/media/video_capabilities.h"

#include "starboard/media.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace starboard {
namespace shared {
namespace starboard {
namespace media {
namespace {

TEST(VideoCapabilitiesTest, EmptyObject) {
  VideoCapabilities video_capabilities;

  EXPECT_FALSE(video_capabilities.IsSupported(
      kSbMediaVideoCodecH264, kSbMediaTransferIdBt709, 640, 480, 30));
  EXPECT_FALSE(video_capabilities.IsSupported(
      kSbMediaVideoCodecVp9, kSbMediaTransferIdBt709, 640, 480, 30));
  EXPECT_FALSE(video_capabilities.IsSupported(
      kSbMediaVideoCodecVp9, kSbMediaTransferId10BitBt2020, 640, 480, 30));
}

TEST(VideoCapabilitiesTest, H264SdrWithOneRule) {
  VideoCapabilities video_capabilities;

  video_capabilities.AddSdrRule(kSbMediaVideoCodecH264, 1920, 1080, 60);

  EXPECT_TRUE(video_capabilities.IsSupported(
      kSbMediaVideoCodecH264, kSbMediaTransferIdBt709, 640, 480, 30));
  EXPECT_TRUE(video_capabilities.IsSupported(
      kSbMediaVideoCodecH264, kSbMediaTransferIdBt709, 1920, 1080, 30));
  EXPECT_TRUE(video_capabilities.IsSupported(
      kSbMediaVideoCodecH264, kSbMediaTransferIdBt709, 1920, 1080, 60));

  EXPECT_FALSE(video_capabilities.IsSupported(
      kSbMediaVideoCodecH264, kSbMediaTransferIdBt709, 640, 480, 61));
  EXPECT_FALSE(video_capabilities.IsSupported(
      kSbMediaVideoCodecH264, kSbMediaTransferIdBt709, 3840, 1080, 30));
  EXPECT_FALSE(video_capabilities.IsSupported(
      kSbMediaVideoCodecH264, kSbMediaTransferIdBt709, 1921, 1080, 30));
  EXPECT_FALSE(video_capabilities.IsSupported(
      kSbMediaVideoCodecH264, kSbMediaTransferIdBt709, 1920, 1081, 30));
  EXPECT_FALSE(video_capabilities.IsSupported(
      kSbMediaVideoCodecH264, kSbMediaTransferIdBt709, 1920, 1080, 61));
  EXPECT_FALSE(video_capabilities.IsSupported(
      kSbMediaVideoCodecH264, kSbMediaTransferIdBt709, 1920, 2160, 30));

  EXPECT_FALSE(video_capabilities.IsSupported(
      kSbMediaVideoCodecH264, kSbMediaTransferId10BitBt2020, 640, 480, 30));

  EXPECT_FALSE(video_capabilities.IsSupported(
      kSbMediaVideoCodecVp9, kSbMediaTransferIdBt709, 640, 480, 30));
}

TEST(VideoCapabilitiesTest, H264SdrWithMixedRules) {
  VideoCapabilities video_capabilities;

  video_capabilities.AddSdrRule(kSbMediaVideoCodecH264, 1920, 1080, 30);
  video_capabilities.AddSdrRule(kSbMediaVideoCodecH264, 1280, 720, 60);

  EXPECT_TRUE(video_capabilities.IsSupported(
      kSbMediaVideoCodecH264, kSbMediaTransferIdBt709, 640, 480, 30));
  EXPECT_TRUE(video_capabilities.IsSupported(
      kSbMediaVideoCodecH264, kSbMediaTransferIdBt709, 640, 480, 60));
  EXPECT_TRUE(video_capabilities.IsSupported(
      kSbMediaVideoCodecH264, kSbMediaTransferIdBt709, 1280, 720, 30));
  EXPECT_TRUE(video_capabilities.IsSupported(
      kSbMediaVideoCodecH264, kSbMediaTransferIdBt709, 1280, 720, 60));
  EXPECT_TRUE(video_capabilities.IsSupported(
      kSbMediaVideoCodecH264, kSbMediaTransferIdBt709, 1920, 1080, 30));

  EXPECT_FALSE(video_capabilities.IsSupported(
      kSbMediaVideoCodecH264, kSbMediaTransferIdBt709, 1920, 1080, 60));
}

TEST(VideoCapabilitiesTest, H264SdrWithRedundantRules) {
  VideoCapabilities video_capabilities;

  video_capabilities.AddSdrRule(kSbMediaVideoCodecH264, 1920, 1080, 60);
  video_capabilities.AddSdrRule(kSbMediaVideoCodecH264, 1920, 1080, 30);

  EXPECT_TRUE(video_capabilities.IsSupported(
      kSbMediaVideoCodecH264, kSbMediaTransferIdBt709, 640, 480, 30));
  EXPECT_TRUE(video_capabilities.IsSupported(
      kSbMediaVideoCodecH264, kSbMediaTransferIdBt709, 1920, 1080, 30));
  EXPECT_TRUE(video_capabilities.IsSupported(
      kSbMediaVideoCodecH264, kSbMediaTransferIdBt709, 1920, 1080, 60));

  EXPECT_FALSE(video_capabilities.IsSupported(
      kSbMediaVideoCodecH264, kSbMediaTransferIdBt709, 640, 480, 61));
  EXPECT_FALSE(video_capabilities.IsSupported(
      kSbMediaVideoCodecH264, kSbMediaTransferIdBt709, 3840, 1080, 30));
  EXPECT_FALSE(video_capabilities.IsSupported(
      kSbMediaVideoCodecH264, kSbMediaTransferIdBt709, 1920, 2160, 30));

  EXPECT_FALSE(video_capabilities.IsSupported(
      kSbMediaVideoCodecH264, kSbMediaTransferId10BitBt2020, 640, 480, 30));

  EXPECT_FALSE(video_capabilities.IsSupported(
      kSbMediaVideoCodecVp9, kSbMediaTransferIdBt709, 640, 480, 30));
}

TEST(VideoCapabilitiesTest, IsSupportedWithZeroValues) {
  VideoCapabilities video_capabilities;

  video_capabilities.AddSdrRule(kSbMediaVideoCodecH264, 1920, 1080, 60);
  video_capabilities.AddHdrRule(kSbMediaVideoCodecH264, 1920, 1080, 60);
  video_capabilities.AddSdrRule(kSbMediaVideoCodecVp9, 1920, 1080, 60);
  video_capabilities.AddHdrRule(kSbMediaVideoCodecVp9, 1920, 1080, 60);

  EXPECT_TRUE(video_capabilities.IsSupported(kSbMediaVideoCodecH264,
                                             kSbMediaTransferIdBt709, 0, 0, 0));
  EXPECT_TRUE(video_capabilities.IsSupported(
      kSbMediaVideoCodecH264, kSbMediaTransferIdBt709, 1920, 0, 0));
  EXPECT_TRUE(video_capabilities.IsSupported(
      kSbMediaVideoCodecH264, kSbMediaTransferIdBt709, 1920, 1080, 0));
  EXPECT_TRUE(video_capabilities.IsSupported(
      kSbMediaVideoCodecH264, kSbMediaTransferIdBt709, 1920, 1080, 60));
  EXPECT_TRUE(video_capabilities.IsSupported(
      kSbMediaVideoCodecH264, kSbMediaTransferIdBt709, 0, 1080, 0));

  EXPECT_TRUE(video_capabilities.IsSupported(
      kSbMediaVideoCodecH264, kSbMediaTransferId10BitBt2020, 0, 0, 0));
  EXPECT_TRUE(video_capabilities.IsSupported(
      kSbMediaVideoCodecH264, kSbMediaTransferId10BitBt2020, 1920, 0, 0));
  EXPECT_TRUE(video_capabilities.IsSupported(
      kSbMediaVideoCodecH264, kSbMediaTransferId10BitBt2020, 1920, 1080, 0));
  EXPECT_TRUE(video_capabilities.IsSupported(
      kSbMediaVideoCodecH264, kSbMediaTransferId10BitBt2020, 1920, 1080, 60));
  EXPECT_TRUE(video_capabilities.IsSupported(
      kSbMediaVideoCodecH264, kSbMediaTransferId10BitBt2020, 0, 1080, 0));

  EXPECT_TRUE(video_capabilities.IsSupported(kSbMediaVideoCodecVp9,
                                             kSbMediaTransferIdBt709, 0, 0, 0));
  EXPECT_TRUE(video_capabilities.IsSupported(
      kSbMediaVideoCodecVp9, kSbMediaTransferIdBt709, 1920, 0, 0));
  EXPECT_TRUE(video_capabilities.IsSupported(
      kSbMediaVideoCodecVp9, kSbMediaTransferIdBt709, 1920, 1080, 0));
  EXPECT_TRUE(video_capabilities.IsSupported(
      kSbMediaVideoCodecVp9, kSbMediaTransferIdBt709, 1920, 1080, 60));
  EXPECT_TRUE(video_capabilities.IsSupported(
      kSbMediaVideoCodecVp9, kSbMediaTransferIdBt709, 0, 1080, 0));

  EXPECT_TRUE(video_capabilities.IsSupported(
      kSbMediaVideoCodecVp9, kSbMediaTransferId10BitBt2020, 0, 0, 0));
  EXPECT_TRUE(video_capabilities.IsSupported(
      kSbMediaVideoCodecVp9, kSbMediaTransferId10BitBt2020, 1920, 0, 0));
  EXPECT_TRUE(video_capabilities.IsSupported(
      kSbMediaVideoCodecVp9, kSbMediaTransferId10BitBt2020, 1920, 1080, 0));
  EXPECT_TRUE(video_capabilities.IsSupported(
      kSbMediaVideoCodecVp9, kSbMediaTransferId10BitBt2020, 1920, 1080, 60));
  EXPECT_TRUE(video_capabilities.IsSupported(
      kSbMediaVideoCodecVp9, kSbMediaTransferId10BitBt2020, 0, 1080, 0));

  EXPECT_TRUE(video_capabilities.IsSupported(
      kSbMediaVideoCodecH264, kSbMediaTransferIdBt709, 1920, 0, 60));
  EXPECT_TRUE(video_capabilities.IsSupported(
      kSbMediaVideoCodecH264, kSbMediaTransferIdBt709, 0, 1080, 60));
  EXPECT_TRUE(video_capabilities.IsSupported(
      kSbMediaVideoCodecH264, kSbMediaTransferId10BitBt2020, 1920, 0, 60));
  EXPECT_TRUE(video_capabilities.IsSupported(
      kSbMediaVideoCodecH264, kSbMediaTransferId10BitBt2020, 0, 1080, 60));

  EXPECT_TRUE(video_capabilities.IsSupported(
      kSbMediaVideoCodecVp9, kSbMediaTransferIdBt709, 1920, 0, 60));
  EXPECT_TRUE(video_capabilities.IsSupported(
      kSbMediaVideoCodecVp9, kSbMediaTransferIdBt709, 0, 1080, 60));
  EXPECT_TRUE(video_capabilities.IsSupported(
      kSbMediaVideoCodecVp9, kSbMediaTransferId10BitBt2020, 1920, 0, 60));
  EXPECT_TRUE(video_capabilities.IsSupported(
      kSbMediaVideoCodecVp9, kSbMediaTransferId10BitBt2020, 0, 1080, 60));
}

TEST(VideoCapabilitiesTest, MultiCodecsAndDynamicRanges) {
  VideoCapabilities video_capabilities;

  video_capabilities.AddSdrRule(kSbMediaVideoCodecH264, 1920, 1080, 60);

  video_capabilities.AddSdrRule(kSbMediaVideoCodecVp9, 3840, 2160, 30);
  video_capabilities.AddSdrRule(kSbMediaVideoCodecVp9, 2560, 1440, 60);

  video_capabilities.AddHdrRule(kSbMediaVideoCodecVp9, 2560, 1440, 30);
  video_capabilities.AddHdrRule(kSbMediaVideoCodecVp9, 1920, 1080, 60);

  EXPECT_TRUE(video_capabilities.IsSupported(
      kSbMediaVideoCodecH264, kSbMediaTransferIdBt709, 640, 480, 30));
  EXPECT_FALSE(video_capabilities.IsSupported(
      kSbMediaVideoCodecH264, kSbMediaTransferId10BitBt2020, 640, 480, 30));
  EXPECT_FALSE(video_capabilities.IsSupported(
      kSbMediaVideoCodecH264, kSbMediaTransferIdBt709, 1920, 1080, 120));
  EXPECT_FALSE(video_capabilities.IsSupported(
      kSbMediaVideoCodecH264, kSbMediaTransferIdBt709, 2560, 1440, 15));

  EXPECT_TRUE(video_capabilities.IsSupported(
      kSbMediaVideoCodecVp9, kSbMediaTransferIdBt709, 3840, 2160, 30));
  EXPECT_FALSE(video_capabilities.IsSupported(
      kSbMediaVideoCodecVp9, kSbMediaTransferIdBt709, 7680, 4320, 15));

  EXPECT_TRUE(video_capabilities.IsSupported(
      kSbMediaVideoCodecVp9, kSbMediaTransferId10BitBt2020, 640, 480, 30));
  EXPECT_TRUE(video_capabilities.IsSupported(
      kSbMediaVideoCodecVp9, kSbMediaTransferIdAribStdB67, 1920, 1080, 60));
  EXPECT_FALSE(video_capabilities.IsSupported(
      kSbMediaVideoCodecVp9, kSbMediaTransferId10BitBt2020, 2560, 1440, 60));
  EXPECT_FALSE(video_capabilities.IsSupported(
      kSbMediaVideoCodecAv1, kSbMediaTransferId10BitBt2020, 2560, 1440, 30));
}

}  // namespace
}  // namespace media
}  // namespace starboard
}  // namespace shared
}  // namespace starboard
