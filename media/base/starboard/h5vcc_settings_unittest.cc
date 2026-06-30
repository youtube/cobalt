// Copyright 2025 The Cobalt Authors. All Rights Reserved.
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

#include "media/base/starboard/h5vcc_settings.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace media {

TEST(H5vccSettingsTest, GetBool) {
  H5vccSettingsMap map;
  map["Media.AllowAudioWritingOnPause"] = "1";
  map["Media.BypassMojoForMedia"] = "0";

  EXPECT_TRUE(kMediaAllowAudioWritingOnPause.GetBool(map));
  EXPECT_FALSE(kMediaBypassMojoForMedia.GetBool(map));
  EXPECT_FALSE(kMediaEnableLowLatency.GetBool(map));
}

TEST(H5vccSettingsTest, GetOptionalBool) {
  H5vccSettingsMap map;
  map["Media.AllowAudioWritingOnPause"] = "1";
  map["Media.BypassMojoForMedia"] = "0";

  EXPECT_EQ(kMediaAllowAudioWritingOnPause.GetOptionalBool(map),
            std::optional<bool>(true));
  EXPECT_EQ(kMediaBypassMojoForMedia.GetOptionalBool(map),
            std::optional<bool>(false));
  EXPECT_EQ(kMediaEnableLowLatency.GetOptionalBool(map), std::nullopt);
}

TEST(H5vccSettingsTest, GetRangedInt) {
  H5vccSettingsMap map;
  map["Media.MaxSamplesPerWrite"] = "50";
  map["Media.VideoDecoderInitialPrerollCount"] = "0";

  EXPECT_EQ(kMediaMaxSamplesPerWrite.GetRangedInt(map, 1, 100),
            std::optional<int>(50));
  EXPECT_EQ(kMediaVideoDecoderInitialPrerollCount.GetRangedInt(map, 1, 100),
            std::nullopt);
  EXPECT_EQ(kMediaVideoRendererMinDecodedFrames.GetRangedInt(map, 1, 100),
            std::nullopt);

  // Test custom sentinel overload (-1)
  map["Media.VideoRendererMinInputBuffers"] = "-1";
  EXPECT_EQ(kMediaVideoRendererMinInputBuffers.GetRangedInt(
                map, 1, 100, /*unset_sentinel=*/-1),
            std::nullopt);
}

}  // namespace media
