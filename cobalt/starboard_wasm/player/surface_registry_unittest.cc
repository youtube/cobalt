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

#include "cobalt/starboard_wasm/player/surface_registry.h"

#include <string>
#include <vector>

#include "testing/gtest/include/gtest/gtest.h"

namespace cobalt {
namespace starboard_wasm {
namespace {

class RecordingObserver : public SurfaceRegistryObserver {
 public:
  void OnSurfaceBoundsChanged(const std::string& token) override {
    tokens.push_back(token);
  }

  std::vector<std::string> tokens;
};

class SurfaceRegistryTest : public testing::Test {
 protected:
  void SetUp() override { registry().AddObserver(&observer_); }

  void TearDown() override {
    registry().RemoveObserver(&observer_);
    for (const std::string& token : registered_) {
      registry().Unregister(token);
    }
  }

  static SurfaceRegistry& registry() { return SurfaceRegistry::Get(); }

  std::string Register(const std::string& token) {
    registry().Register(token);
    registered_.push_back(token);
    return token;
  }

  RecordingObserver observer_;
  std::vector<std::string> registered_;
};

TEST_F(SurfaceRegistryTest, IgnoresUnregisteredTokens) {
  registry().SetBounds("unknown", PlaneRect{1, 2, 3, 4});
  PlaneRect rect;
  EXPECT_FALSE(registry().Bounds("unknown", &rect));
  EXPECT_TRUE(observer_.tokens.empty());
}

TEST_F(SurfaceRegistryTest, SetBoundsNotifiesOncePerChange) {
  const std::string token = Register("token-a");
  PlaneRect rect;
  EXPECT_FALSE(registry().Bounds(token, &rect));

  registry().SetBounds(token, PlaneRect{10, 20, 300, 400});
  registry().SetBounds(token, PlaneRect{10, 20, 300, 400});
  ASSERT_TRUE(registry().Bounds(token, &rect));
  EXPECT_EQ(rect, (PlaneRect{10, 20, 300, 400}));
  EXPECT_EQ(observer_.tokens, std::vector<std::string>({token}));

  registry().ClearBounds(token);
  registry().ClearBounds(token);
  EXPECT_FALSE(registry().Bounds(token, &rect));
  EXPECT_EQ(observer_.tokens, std::vector<std::string>({token, token}));
}

TEST_F(SurfaceRegistryTest, KeepsSurfacesSeparate) {
  const std::string a = Register("token-a");
  const std::string b = Register("token-b");
  registry().SetBounds(a, PlaneRect{0, 0, 100, 100});

  PlaneRect rect;
  EXPECT_TRUE(registry().Bounds(a, &rect));
  EXPECT_FALSE(registry().Bounds(b, &rect));
}

TEST_F(SurfaceRegistryTest, ReportsVideoSizeOnlyWhenKnown) {
  const std::string token = Register("token-a");
  int32_t width = 0;
  int32_t height = 0;
  EXPECT_FALSE(registry().VideoSize(token, &width, &height));

  registry().SetVideoSize(token, 1080, 1920);
  ASSERT_TRUE(registry().VideoSize(token, &width, &height));
  EXPECT_EQ(width, 1080);
  EXPECT_EQ(height, 1920);
}

TEST_F(SurfaceRegistryTest, UnregisterDropsState) {
  const std::string token = Register("token-a");
  registry().SetBounds(token, PlaneRect{0, 0, 100, 100});
  registry().Unregister(token);

  PlaneRect rect;
  EXPECT_FALSE(registry().Bounds(token, &rect));
  registry().SetBounds(token, PlaneRect{0, 0, 50, 50});
  EXPECT_FALSE(registry().Bounds(token, &rect));
}

}  // namespace
}  // namespace starboard_wasm
}  // namespace cobalt
