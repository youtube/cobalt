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

#include "starboard/android/shared/protected_video_plane.h"

#include <vector>

#include "testing/gtest/include/gtest/gtest.h"

namespace starboard {
namespace {

// Records the hook calls made with it as context.
struct FakeEmbedder {
  bool create_result = true;
  std::vector<uint32_t> deleted;
  std::vector<uint32_t> updated;
  int last_width = 0;
  int last_height = 0;

  static bool Create(void* context,
                     uint32_t* out_texture,
                     uint32_t* out_native_texture) {
    auto* self = static_cast<FakeEmbedder*>(context);
    *out_texture = 7;
    *out_native_texture = 42;
    return self->create_result;
  }
  static void Delete(void* context, uint32_t texture) {
    static_cast<FakeEmbedder*>(context)->deleted.push_back(texture);
  }
  static void AfterUpdate(void* context,
                          uint32_t texture,
                          int width,
                          int height) {
    auto* self = static_cast<FakeEmbedder*>(context);
    self->updated.push_back(texture);
    self->last_width = width;
    self->last_height = height;
  }

  StarboardProtectedDecodeTargetTextureHooks hooks() {
    return {this, &Create, &Delete, &AfterUpdate};
  }
};

const StarboardExtensionProtectedVideoPlaneApi* Api() {
  return static_cast<const StarboardExtensionProtectedVideoPlaneApi*>(
      GetProtectedVideoPlaneApi());
}

class ProtectedVideoPlaneTest : public ::testing::Test {
 protected:
  ~ProtectedVideoPlaneTest() override {
    Api()->SetDecodeTargetTextureHooks(&provider_, nullptr);
    Api()->SetDecodeTargetTextureHooks(&other_provider_, nullptr);
  }

  SbDecodeTargetGraphicsContextProvider provider_ = {};
  SbDecodeTargetGraphicsContextProvider other_provider_ = {};
  FakeEmbedder embedder_;
};

TEST_F(ProtectedVideoPlaneTest, NoHooksMeansSystemGlTextures) {
  EXPECT_EQ(GetProtectedDecodeTargetTextureHooksId(&provider_), 0u);
}

TEST_F(ProtectedVideoPlaneTest, HooksReceiveTheirContext) {
  const StarboardProtectedDecodeTargetTextureHooks hooks = embedder_.hooks();
  Api()->SetDecodeTargetTextureHooks(&provider_, &hooks);
  const uint64_t id = GetProtectedDecodeTargetTextureHooksId(&provider_);
  ASSERT_NE(id, 0u);

  uint32_t texture = 0;
  uint32_t native_texture = 0;
  ASSERT_TRUE(CreateProtectedDecodeTargetTexture(&provider_, id, &texture,
                                                 &native_texture));
  EXPECT_EQ(texture, 7u);
  EXPECT_EQ(native_texture, 42u);

  NotifyProtectedDecodeTargetUpdated(&provider_, id, texture, 3840, 2160);
  EXPECT_EQ(embedder_.updated, std::vector<uint32_t>{7});
  EXPECT_EQ(embedder_.last_width, 3840);
  EXPECT_EQ(embedder_.last_height, 2160);

  DeleteProtectedDecodeTargetTexture(&provider_, id, texture);
  EXPECT_EQ(embedder_.deleted, std::vector<uint32_t>{7});
}

TEST_F(ProtectedVideoPlaneTest, CreateFailureIsReported) {
  embedder_.create_result = false;
  const StarboardProtectedDecodeTargetTextureHooks hooks = embedder_.hooks();
  Api()->SetDecodeTargetTextureHooks(&provider_, &hooks);
  uint32_t texture = 0;
  uint32_t native_texture = 0;
  EXPECT_FALSE(CreateProtectedDecodeTargetTexture(
      &provider_, GetProtectedDecodeTargetTextureHooksId(&provider_), &texture,
      &native_texture));
}

TEST_F(ProtectedVideoPlaneTest, HooksArePerProvider) {
  const StarboardProtectedDecodeTargetTextureHooks hooks = embedder_.hooks();
  Api()->SetDecodeTargetTextureHooks(&provider_, &hooks);
  const uint64_t id = GetProtectedDecodeTargetTextureHooksId(&provider_);
  EXPECT_EQ(GetProtectedDecodeTargetTextureHooksId(&other_provider_), 0u);
  DeleteProtectedDecodeTargetTexture(&other_provider_, id, 7);
  EXPECT_TRUE(embedder_.deleted.empty());
}

TEST_F(ProtectedVideoPlaneTest, NoHookRunsAfterRemoval) {
  const StarboardProtectedDecodeTargetTextureHooks hooks = embedder_.hooks();
  Api()->SetDecodeTargetTextureHooks(&provider_, &hooks);
  const uint64_t id = GetProtectedDecodeTargetTextureHooksId(&provider_);
  Api()->SetDecodeTargetTextureHooks(&provider_, nullptr);

  EXPECT_EQ(GetProtectedDecodeTargetTextureHooksId(&provider_), 0u);
  // A decode target outliving its embedder: the embedder already deleted the
  // texture, so nothing is called.
  DeleteProtectedDecodeTargetTexture(&provider_, id, 7);
  NotifyProtectedDecodeTargetUpdated(&provider_, id, 7, 640, 360);
  EXPECT_TRUE(embedder_.deleted.empty());
  EXPECT_TRUE(embedder_.updated.empty());
}

TEST_F(ProtectedVideoPlaneTest, ReinstalledHooksIgnoreOlderTextures) {
  const StarboardProtectedDecodeTargetTextureHooks hooks = embedder_.hooks();
  Api()->SetDecodeTargetTextureHooks(&provider_, &hooks);
  const uint64_t old_id = GetProtectedDecodeTargetTextureHooksId(&provider_);
  // E.g. after the embedder replaced its context and deleted its textures,
  // whose names the new context may reuse.
  Api()->SetDecodeTargetTextureHooks(&provider_, &hooks);
  const uint64_t new_id = GetProtectedDecodeTargetTextureHooksId(&provider_);
  EXPECT_NE(new_id, old_id);

  DeleteProtectedDecodeTargetTexture(&provider_, old_id, 7);
  EXPECT_TRUE(embedder_.deleted.empty());
  DeleteProtectedDecodeTargetTexture(&provider_, new_id, 7);
  EXPECT_EQ(embedder_.deleted, std::vector<uint32_t>{7});
}

TEST_F(ProtectedVideoPlaneTest, HooksAreCopied) {
  {
    const StarboardProtectedDecodeTargetTextureHooks hooks = embedder_.hooks();
    Api()->SetDecodeTargetTextureHooks(&provider_, &hooks);
  }
  DeleteProtectedDecodeTargetTexture(
      &provider_, GetProtectedDecodeTargetTextureHooksId(&provider_), 9);
  EXPECT_EQ(embedder_.deleted, std::vector<uint32_t>{9});
}

}  // namespace
}  // namespace starboard
