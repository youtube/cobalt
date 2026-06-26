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

#include "starboard/shared/starboard/experimental_features.h"

#include <optional>
#include <type_traits>

#include "starboard/common/log.h"
#include "starboard/common/no_destructor.h"
#include "starboard/extension/experimental/experimental_features.h"
#include "starboard/shared/starboard/player/decoded_audio_internal.h"

namespace starboard {

namespace {

thread_local NoDestructor<std::optional<ExperimentalFeatures>>
    g_experimental_features;
static_assert(sizeof(ExperimentalFeatures) < 256,
              "ExperimentalFeatures is too large for thread-local storage.");
static_assert(
    std::is_trivially_destructible<decltype(g_experimental_features)>::value,
    "g_experimental_features must be trivially destructible.");

const StarboardExtensionExperimentalFeaturesConfigurationApi
    kExperimentalFeaturesConfigurationApi = {
        kStarboardExtensionExperimentalFeaturesConfigurationName,
        1,
        &SetExperimentalFeaturesForCurrentThread,
};

}  // namespace

ExperimentalFeatures::ExperimentalFeatures(const Map& settings)
    : settings_(settings) {}

bool ExperimentalFeatures::GetBool(const ExperimentalFeatureKey& key) const {
  return Get<bool>(key).value_or(false);
}

void SetExperimentalFeaturesForCurrentThread(
    const StarboardExtensionExperimentalFeatures* extension_features) {
  // |extension_features| cannot be null. We use a pointer here to support C API
  // compatibility.
  SB_CHECK(extension_features);
  SB_CHECK(extension_features->settings_map);

  *g_experimental_features =
      ExperimentalFeatures(*static_cast<const ExperimentalFeatures::Map*>(
          extension_features->settings_map));

  if ((*g_experimental_features)
          ->GetBool(kMediaEnableSimdBasedAudioFormatSwitching)) {
    DecodedAudio::EnableSimdBasedAudioFormatSwitching();
  }
}

const ExperimentalFeatures& GetExperimentalFeaturesForCurrentThread() {
  SB_CHECK((*g_experimental_features).has_value())
      << "ExperimentalFeatures are not set. This method was likely "
         "called on the wrong thread or a race condition occurred.";
  return **g_experimental_features;
}

const void* GetExperimentalFeaturesConfigurationApi() {
  return &kExperimentalFeaturesConfigurationApi;
}

}  // namespace starboard
