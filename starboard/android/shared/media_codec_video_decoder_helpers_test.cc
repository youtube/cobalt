// Copyright 2026 The Cobalt Authors. All Rights Reserved.
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

#include "starboard/android/shared/media_codec_video_decoder_helpers.h"

#include <array>
#include <optional>

#include "testing/gtest/include/gtest/gtest.h"

namespace starboard {
namespace {

TEST(MediaCodecVideoDecoderHelpersTest, IsSoftwareDecodeRequired) {
  // Blank capabilities
  EXPECT_FALSE(IsSoftwareDecodeRequired(""));

  // Explicitly required/preferred
  EXPECT_TRUE(IsSoftwareDecodeRequired("softwaredecoder=required"));
  EXPECT_TRUE(IsSoftwareDecodeRequired("softwaredecoder=preferred"));

  // Explicitly disallowed/unpreferred
  EXPECT_FALSE(IsSoftwareDecodeRequired("softwaredecoder=disallowed"));
  EXPECT_FALSE(IsSoftwareDecodeRequired("softwaredecoder=unpreferred"));

  // Low resolution + low fps (should trigger fallback to software)
  EXPECT_TRUE(IsSoftwareDecodeRequired("width=432; height=240; framerate=15"));

  // High resolution (should trigger hardware)
  EXPECT_FALSE(
      IsSoftwareDecodeRequired("width=1920; height=1080; framerate=30"));
}

TEST(MediaCodecVideoDecoderHelpersTest, ParseMaxResolution) {
  Size frame_size = {1920, 1080};

  // Both dimensions provided
  auto res = ParseMaxResolution("width=1280; height=720", frame_size);
  ASSERT_TRUE(res.has_value());
  EXPECT_EQ(res->width, 1280);
  EXPECT_EQ(res->height, 720);

  // Only width provided (infer height)
  res = ParseMaxResolution("width=1280", frame_size);
  ASSERT_TRUE(res.has_value());
  EXPECT_EQ(res->width, 1280);
  EXPECT_EQ(res->height, 720);  // 1280 * 1080 / 1920 = 720

  // Only height provided (infer width)
  res = ParseMaxResolution("height=720", frame_size);
  ASSERT_TRUE(res.has_value());
  EXPECT_EQ(res->width, 1280);
  EXPECT_EQ(res->height, 720);
}

TEST(MediaCodecVideoDecoderHelpersTest, GetDecodeTargetGeometryFromMatrix) {
  // Identity 4x4 matrix
  std::array<float, 16> identity = {1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f,
                                    0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f,
                                    0.0f, 0.0f, 0.0f, 1.0f};
  Size display_size = {1920, 1080};

  auto geom = GetDecodeTargetGeometryFromMatrix(identity, display_size);
  EXPECT_EQ(geom.coded_size.width, 1920);
  EXPECT_EQ(geom.coded_size.height, 1080);

  // Identity matrix has raw_sy = 1.0f (> 0), which means it includes vertical
  // flip. Therefore left/right remain, but top/bottom are swapped.
  EXPECT_EQ(geom.content_region.left, 0);
  EXPECT_EQ(geom.content_region.right, 1920);
  EXPECT_EQ(geom.content_region.top, 1080);
  EXPECT_EQ(geom.content_region.bottom, 0);
}

}  // namespace
}  // namespace starboard
