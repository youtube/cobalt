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

#include <cerrno>
#include <cstdlib>
#include <optional>
#include <utility>

#include "starboard/common/log.h"
#include "starboard/extension/experimental/experimental_features.h"

namespace starboard {

namespace {

thread_local std::optional<ExperimentalFeatures> g_experimental_features;

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
  auto it = settings_.find(key.key());
  if (it == settings_.end()) {
    return false;
  }
  return it->second != "0" && it->second != "false";
}

std::optional<bool> ExperimentalFeatures::GetOptionalBool(
    const ExperimentalFeatureKey& key) const {
  auto it = settings_.find(key.key());
  if (it == settings_.end()) {
    return std::nullopt;
  }
  return it->second != "0" && it->second != "false";
}

std::optional<int> ExperimentalFeatures::GetRangedInt(
    const ExperimentalFeatureKey& key,
    int min_val,
    int max_val) const {
  auto it = settings_.find(key.key());
  if (it == settings_.end()) {
    return std::nullopt;
  }
  char* end = nullptr;
  errno = 0;
  long val = std::strtol(it->second.c_str(), &end, 10);
  if (errno == ERANGE || end == it->second.c_str() || *end != '\0') {
    return std::nullopt;
  }
  if (val == 0 || val < min_val || val > max_val) {
    return std::nullopt;
  }
  return static_cast<int>(val);
}

void SetExperimentalFeaturesForCurrentThread(
    const StarboardExtensionExperimentalFeatures* extension_features) {
  SB_CHECK(extension_features);
  SB_CHECK(extension_features->settings_map);

  g_experimental_features.emplace(
      *static_cast<const ExperimentalFeatures::Map*>(
          extension_features->settings_map));
}

const ExperimentalFeatures& GetExperimentalFeaturesForCurrentThread() {
  SB_CHECK(g_experimental_features.has_value())
      << "ExperimentalFeatures are not set for the current thread.";
  return *g_experimental_features;
}

const void* GetExperimentalFeaturesConfigurationApi() {
  return &kExperimentalFeaturesConfigurationApi;
}

}  // namespace starboard
