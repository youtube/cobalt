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

#include "media/base/starboard/experimental_features.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace media {

TEST(ExperimentalFeaturesTest, GetBool) {
  ExperimentalFeatures::Map map;
  map["Media.ForceDecodeToTexture"] = 1;
  map["Media.BypassMojoForMedia"] = 0;
  ExperimentalFeatures settings(map);

  EXPECT_TRUE(settings.GetBool(kMediaForceDecodeToTexture));
  EXPECT_FALSE(settings.GetBool(kMediaBypassMojoForMedia));
  EXPECT_FALSE(settings.GetBool(kMediaForceClearSurfaceView));
}

TEST(ExperimentalFeaturesTest, GenericGet) {
  ExperimentalFeatures::Map map;
  map["Media.ForceDecodeToTexture"] = 1;
  map["Media.BypassMojoForMedia"] = 0;
  map["Media.MaxSamplesPerWrite"] = 50;
  ExperimentalFeatures settings(map);

  EXPECT_EQ(settings.Get(kMediaForceDecodeToTexture),
            std::optional<bool>(true));
  EXPECT_EQ(settings.Get(kMediaBypassMojoForMedia), std::optional<bool>(false));
  EXPECT_EQ(settings.Get(kMediaMaxSamplesPerWrite), std::optional<int>(50));
}

}  // namespace media
