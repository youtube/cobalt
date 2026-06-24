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

#include "starboard/shared/starboard/media/resolutions.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace starboard {
namespace {

TEST(MediaCodecVideoDecoderHelpersTest, IsSoftwareDecoderRequired) {
  // Blank capabilities
  EXPECT_FALSE(IsSoftwareDecoderRequired(""));

  // Explicitly required/preferred
  EXPECT_TRUE(IsSoftwareDecoderRequired("softwaredecoder=required"));
  EXPECT_TRUE(IsSoftwareDecoderRequired("softwaredecoder=preferred"));

  // Explicitly disallowed/unpreferred
  EXPECT_FALSE(IsSoftwareDecoderRequired("softwaredecoder=disallowed"));
  EXPECT_FALSE(IsSoftwareDecoderRequired("softwaredecoder=unpreferred"));

  // Low resolution + low fps (should trigger fallback to software)
  EXPECT_TRUE(IsSoftwareDecoderRequired("width=432; height=240; framerate=15"));

  // High resolution (should trigger hardware)
  EXPECT_FALSE(
      IsSoftwareDecoderRequired("width=1920; height=1080; framerate=30"));
}

TEST(MediaCodecVideoDecoderHelpersTest, ParseMaxResolution) {
  Size frame_size = Resolution::k1080p;

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

TEST(MediaCodecVideoDecoderHelpersTest, EqualAndIsIdentity) {
  // Identity color metadata
  SbMediaColorMetadata identity = {};
  identity.primaries = kSbMediaPrimaryIdBt709;
  identity.transfer = kSbMediaTransferIdBt709;
  identity.matrix = kSbMediaMatrixIdBt709;
  identity.range = kSbMediaRangeIdLimited;
  EXPECT_TRUE(IsIdentity(identity));

  // Non-identity primaries
  SbMediaColorMetadata non_identity_primaries = identity;
  non_identity_primaries.primaries = kSbMediaPrimaryIdBt2020;
  EXPECT_FALSE(IsIdentity(non_identity_primaries));

  // Non-identity transfer
  SbMediaColorMetadata non_identity_transfer = identity;
  non_identity_transfer.transfer = kSbMediaTransferIdSmpteSt2084;
  EXPECT_FALSE(IsIdentity(non_identity_transfer));

  // Non-empty mastering metadata
  SbMediaColorMetadata non_empty_mastering = identity;
  non_empty_mastering.mastering_metadata.luminance_max = 1000.0f;
  EXPECT_FALSE(IsIdentity(non_empty_mastering));
}

TEST(MediaCodecVideoDecoderHelpersTest, GetDecodeTargetGeometryFromMatrix) {
  // Identity 4x4 matrix
  std::array<float, 16> identity = {1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f,
                                    0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f,
                                    0.0f, 0.0f, 0.0f, 1.0f};
  Size display_size = Resolution::k1080p;

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

TEST(MediaCodecVideoDecoderHelpersTest,
     GetDecodeTargetGeometryFromMatrix_HorizontalFlip) {
  // Matrix representing vertical flip AND horizontal flip (raw_sx = -1.0f,
  // raw_sy = 1.0f) Standard GL texture coordinates for horizontal/vertical flip
  // would have tx=1.0f, ty=0.0f.
  std::array<float, 16> flipped = {-1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f,
                                   0.0f,  0.0f, 0.0f, 0.0f, 1.0f, 0.0f,
                                   1.0f,  0.0f, 0.0f, 1.0f};
  Size display_size = Resolution::k1080p;

  auto geom = GetDecodeTargetGeometryFromMatrix(flipped, display_size);
  EXPECT_EQ(geom.coded_size.width, 1920);
  EXPECT_EQ(geom.coded_size.height, 1080);

  // raw_sx < 0 -> horizontally flipped -> left and right are swapped.
  // raw_sy > 0 -> vertically flipped -> top and bottom are swapped.
  EXPECT_EQ(geom.content_region.left, 1920);
  EXPECT_EQ(geom.content_region.right, 0);
  EXPECT_EQ(geom.content_region.top, 1080);
  EXPECT_EQ(geom.content_region.bottom, 0);
}

TEST(MediaCodecVideoDecoderHelpersTest,
     GetDecodeTargetGeometryFromMatrix_Scaling) {
  // Matrix representing 2x scale (upscale texture coordinates, meaning
  // downscale of display) sx = 0.5f, sy = 0.5f. Coded size should become 2x
  // display size.
  std::array<float, 16> scaled = {0.5f, 0.0f, 0.0f, 0.0f, 0.0f, 0.5f,
                                  0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f,
                                  0.0f, 0.0f, 0.0f, 1.0f};
  Size display_size = Resolution::k1080p;

  auto geom = GetDecodeTargetGeometryFromMatrix(scaled, display_size);
  // 2x scaling means coded_size resolves to 3840x2160.
  EXPECT_EQ(geom.coded_size.width, 3840);
  EXPECT_EQ(geom.coded_size.height, 2160);

  // raw_sy > 0 -> top/bottom swapped
  EXPECT_EQ(geom.content_region.left, 0);
  EXPECT_EQ(geom.content_region.right, 1920);
  EXPECT_EQ(geom.content_region.top, 1080);
  EXPECT_EQ(geom.content_region.bottom, 0);
}

TEST(MediaCodecVideoDecoderHelpersTest,
     GetDecodeTargetGeometryFromMatrix_Translation) {
  // Matrix representing translation / offset (tx = 0.2f, ty = 0.1f)
  // Identity scale (sx = 1.0f, sy = 1.0f).
  std::array<float, 16> translated = {0.8f, 0.0f, 0.0f, 0.0f, 0.0f, 0.8f,
                                      0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f,
                                      0.1f, 0.1f, 0.0f, 1.0f};
  Size display_size = Resolution::k1080p;

  auto geom = GetDecodeTargetGeometryFromMatrix(translated, display_size);
  EXPECT_EQ(geom.coded_size.width, 2398);
  EXPECT_EQ(geom.coded_size.height, 1348);

  // Scaling sx = 0.8f, sy = 0.8f, tx = 0.1f, ty = 0.1f.
  // The algorithm evaluates shrink_amount = 1.0f first, which yields
  // visible_x/y >= 0 and exits: coded_size.width = round((1920 - 2) / 0.8) =
  // 2398 visible_x = round(0.1 * 2398 - 1.0) = 239 coded_size.height =
  // round((1080 - 2) / 0.8) = 1348 visible_y = round(0.1 * 1348 - 1.0) = 134
  // raw_sy > 0 -> top/bottom swapped.
  EXPECT_EQ(geom.content_region.left, 239);
  EXPECT_EQ(geom.content_region.right, 239 + 1920);
  EXPECT_EQ(geom.content_region.top, 134 + 1080);
  EXPECT_EQ(geom.content_region.bottom, 134);
}

}  // namespace
}  // namespace starboard
