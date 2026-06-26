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
  map["Media.ForceDecodeToTexture"] = "1";
  map["Media.BypassMojoForMedia"] = "0";

  EXPECT_TRUE(kMediaForceDecodeToTexture.GetBool(map));
  EXPECT_FALSE(kMediaBypassMojoForMedia.GetBool(map));
  EXPECT_FALSE(kMediaMaxSamplesPerWrite.GetBool(map));
}

TEST(H5vccSettingsTest, GenericGet) {
  H5vccSettingsMap map;
  map["Media.ForceDecodeToTexture"] = "1";
  map["Media.BypassMojoForMedia"] = "0";
  map["Media.MaxSamplesPerWrite"] = "50";

  EXPECT_EQ(kMediaForceDecodeToTexture.Get<bool>(map),
            std::optional<bool>(true));
  EXPECT_EQ(kMediaBypassMojoForMedia.Get<bool>(map),
            std::optional<bool>(false));
  EXPECT_EQ(kMediaMaxSamplesPerWrite.Get<int>(map), std::optional<int>(50));
}

}  // namespace media
