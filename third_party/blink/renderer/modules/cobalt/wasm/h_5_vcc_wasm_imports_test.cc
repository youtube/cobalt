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

#include "third_party/blink/renderer/modules/cobalt/wasm/h_5_vcc_wasm_imports.h"

#include <memory>
#include <optional>
#include <string>

#include "cobalt/starboard_wasm/wasm_player_host.h"
#include "starboard/extension/experimental/experimental_features.h"
#include "starboard/extension/player_settings.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/testing/dummy_page_holder.h"
#include "third_party/blink/renderer/modules/cobalt/wasm/h_5_vcc_wasm_player.h"
#include "third_party/blink/renderer/modules/cobalt/wasm/h_5_vcc_wasm_surface.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"

namespace blink {
namespace {

using cobalt::starboard_wasm::WasmPlayerHost;

// The extension calls made on this thread, or nullopt for none.
std::optional<size_t>& FeatureCount() {
  static std::optional<size_t> count;
  return count;
}
std::optional<int>& MaxVideoInputSize() {
  static std::optional<int> size;
  return size;
}
std::optional<std::string>& MaxVideoResolution() {
  static std::optional<std::string> resolution;
  return resolution;
}

constexpr StarboardExtensionExperimentalFeaturesConfigurationApi kFeaturesApi =
    {kStarboardExtensionExperimentalFeaturesConfigurationName, 1,
     [](const StarboardExtensionExperimentalFeatures* features) {
       FeatureCount() = features->entry_count;
     }};

constexpr StarboardExtensionPlayerSettingsApi kPlayerSettingsApi = {
    kStarboardExtensionPlayerSettingsName, 1,
    [](int size) { MaxVideoInputSize() = size; },
    [](const char* resolution) {
      MaxVideoResolution() = resolution ? resolution : "<null>";
    },
    nullptr};

class H5vccWasmImportsTest : public testing::Test {
 protected:
  H5vccWasmImportsTest() {
    WasmPlayerHost::SetExperimentalFeaturesApiForTesting(&kFeaturesApi);
    WasmPlayerHost::SetPlayerSettingsApiForTesting(&kPlayerSettingsApi);
    FeatureCount().reset();
    MaxVideoInputSize().reset();
    MaxVideoResolution().reset();
  }
  ~H5vccWasmImportsTest() override {
    WasmPlayerHost::SetPlayerSettingsApiForTesting(nullptr);
    WasmPlayerHost::SetExperimentalFeaturesApiForTesting(nullptr);
  }

  test::TaskEnvironment task_environment_;
  std::unique_ptr<DummyPageHolder> page_holder_ =
      std::make_unique<DummyPageHolder>();
};

TEST_F(H5vccWasmImportsTest, ContextDestroyedResetsThreadSettings) {
  auto* imports = MakeGarbageCollected<H5vccWasmImports>(
      *page_holder_->GetFrame().DomWindow());
  H5vccWasmPlayer* player = imports->player();
  ASSERT_TRUE(player->setMaxVideoInputSizeForCurrentThread(1024));
  EXPECT_EQ(MaxVideoInputSize(), 1024);
  EXPECT_FALSE(FeatureCount());

  page_holder_.reset();
  EXPECT_FALSE(imports->host());
  EXPECT_EQ(FeatureCount(), 0u);
  EXPECT_EQ(MaxVideoInputSize(), 0);
  EXPECT_EQ(MaxVideoResolution(), "<null>");
}

TEST_F(H5vccWasmImportsTest, OperationsValidateHandles) {
  auto* imports = MakeGarbageCollected<H5vccWasmImports>(
      *page_holder_->GetFrame().DomWindow());
  H5vccWasmPlayer* player = imports->player();
  H5vccWasmSurface* surface = imports->surface();
  EXPECT_FALSE(player->seek(/*player=*/7, 0, 0));
  EXPECT_FALSE(player->writeEndOfStream(/*player=*/7, /*stream_type=*/0));
  EXPECT_FALSE(player->setPlaybackRate(/*player=*/7, 1));
  EXPECT_EQ(player->getMaximumNumberOfSamplesPerWrite(/*player=*/7, 0), 0);
  const uint32_t id = surface->create();
  EXPECT_NE(id, 0u);
  EXPECT_EQ(surface->token(id).length(), 32u);
  EXPECT_TRUE(surface->token(id + 1).empty());
  surface->destroy(id);
  EXPECT_TRUE(surface->token(id).empty());

  // Once the context is gone, operations do nothing.
  page_holder_.reset();
  EXPECT_EQ(surface->create(), 0u);
  EXPECT_FALSE(player->setMaxVideoInputSizeForCurrentThread(1));
}

}  // namespace
}  // namespace blink
