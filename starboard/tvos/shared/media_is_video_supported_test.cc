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

#include "starboard/media.h"
#include "starboard/shared/starboard/feature_list.h"
#include "starboard/shared/starboard/features.h"
#include "starboard/shared/starboard/media/media_support_internal.h"
#include "starboard/shared/starboard/media/resolutions.h"
#include "starboard/testing/scoped_feature_list.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace starboard {
namespace {

bool Is1080pSdrH264SupportedAtFps(int fps) {
  VideoSupportQueryParams params;
  params.codec_info.codec = kSbMediaVideoCodecH264;
  params.codec_info.primary_id = kSbMediaPrimaryIdBt709;
  params.codec_info.transfer_id = kSbMediaTransferIdBt709;
  params.codec_info.matrix_id = kSbMediaMatrixIdBt709;
  params.frame_size = Resolution::k1080p;
  params.bitrate = 5'000'000;
  params.fps = fps;
  return MediaIsVideoSupported(/*mime_type=*/nullptr, params);
}

TEST(MediaIsVideoSupportedTest, DefaultFpsCapIs60) {
  EXPECT_FALSE(features::FeatureList::IsEnabled(features::kSupport120FpsVideo));
  EXPECT_TRUE(Is1080pSdrH264SupportedAtFps(60));
  EXPECT_FALSE(Is1080pSdrH264SupportedAtFps(61));
  EXPECT_FALSE(Is1080pSdrH264SupportedAtFps(120));
}

TEST(MediaIsVideoSupportedTest, RaisesFpsCapTo120WhenFeatureEnabled) {
  testing::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(features::kSupport120FpsVideo);

  EXPECT_TRUE(Is1080pSdrH264SupportedAtFps(60));
  EXPECT_TRUE(Is1080pSdrH264SupportedAtFps(61));
  EXPECT_TRUE(Is1080pSdrH264SupportedAtFps(120));
  EXPECT_FALSE(Is1080pSdrH264SupportedAtFps(121));
}

TEST(MediaIsVideoSupportedTest, CapsFpsAt60WhenFeatureDisabled) {
  testing::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(features::kSupport120FpsVideo);

  EXPECT_TRUE(Is1080pSdrH264SupportedAtFps(60));
  EXPECT_FALSE(Is1080pSdrH264SupportedAtFps(61));
  EXPECT_FALSE(Is1080pSdrH264SupportedAtFps(120));
}

}  // namespace
}  // namespace starboard
