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

#include "cobalt/starboard_wasm/player/player_settings.h"

#include "base/test/bind.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cobalt {
namespace starboard_wasm {
namespace {

TEST(PlayerSettingsTest, DefaultsWithoutProvider) {
  SetPlayerSettingsProvider(PlayerSettingsProvider());
  const PlayerSettings settings = GetPlayerSettings();
  EXPECT_TRUE(settings.experimental_features.empty());
  EXPECT_EQ(settings.max_samples_per_write, 0);
  EXPECT_EQ(settings.viewport_width, 0);
  EXPECT_EQ(settings.viewport_height, 0);
}

TEST(PlayerSettingsTest, RunsProviderOnEachGet) {
  int calls = 0;
  SetPlayerSettingsProvider(base::BindLambdaForTesting([&calls] {
    PlayerSettings settings;
    settings.experimental_features["Media.Foo"] = ++calls;
    settings.max_samples_per_write = 4;
    return settings;
  }));
  EXPECT_EQ(GetPlayerSettings().experimental_features.at("Media.Foo"), 1);
  const PlayerSettings settings = GetPlayerSettings();
  EXPECT_EQ(settings.experimental_features.at("Media.Foo"), 2);
  EXPECT_EQ(settings.max_samples_per_write, 4);
  SetPlayerSettingsProvider(PlayerSettingsProvider());
}

}  // namespace
}  // namespace starboard_wasm
}  // namespace cobalt
