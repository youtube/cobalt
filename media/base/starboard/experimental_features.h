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

#ifndef MEDIA_BASE_STARBOARD_EXPERIMENTAL_FEATURES_H_
#define MEDIA_BASE_STARBOARD_EXPERIMENTAL_FEATURES_H_

#include <cstdint>
#include <functional>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <type_traits>
#include <variant>

#include "media/base/media_export.h"

namespace media {

template <typename T>
class MEDIA_EXPORT ExperimentalFeatureKey {
 public:
  using ValueType = T;

  constexpr explicit ExperimentalFeatureKey(std::string_view key) : key_(key) {}

  constexpr std::string_view key() const { return key_; }

 private:
  std::string_view key_;
};

class MEDIA_EXPORT ExperimentalFeatures {
 public:
  using Value = std::variant<int64_t, std::string>;
  using Map = std::map<std::string, Value, std::less<>>;

  ExperimentalFeatures() = default;
  ExperimentalFeatures(Map settings) : settings_(std::move(settings)) {}
  ExperimentalFeatures& operator=(Map settings) {
    settings_ = std::move(settings);
    return *this;
  }
  ~ExperimentalFeatures() = default;

  // Returns the boolean value for the given key, falling back to false if the
  // key is missing or unset.
  bool GetBool(const ExperimentalFeatureKey<bool>& key) const {
    return Get(key).value_or(false);
  }

  template <typename T>
  std::optional<T> Get(const ExperimentalFeatureKey<T>& key) const {
    auto it = settings_.find(key.key());
    if (it == settings_.end()) {
      return std::nullopt;
    }
    if constexpr (std::is_same_v<T, bool>) {
      if (auto* int_val = std::get_if<int64_t>(&it->second)) {
        return *int_val != 0;
      }
      return std::nullopt;
    } else if constexpr (std::is_same_v<T, int>) {
      if (auto* int_val = std::get_if<int64_t>(&it->second)) {
        int64_t val = *int_val;
        // Experiment framework uses 0 as the sentinel value for unset.
        // http://go/latestexpcl/player_web/features/player_web_cobalt.impl.gcl;l=332;rcl=862772714
        constexpr int kH5vccUnsetSentinel = 0;
        if (val == kH5vccUnsetSentinel) {
          return std::nullopt;
        }
        return static_cast<int>(val);
      }
      return std::nullopt;
    } else if constexpr (std::is_same_v<T, std::string>) {
      if (auto* str_val = std::get_if<std::string>(&it->second)) {
        return *str_val;
      }
      return std::nullopt;
    } else {
      static_assert(sizeof(T) == 0,
                    "Unsupported type for ExperimentalFeatures::Get");
      return std::nullopt;
    }
  }

  const Map& settings() const { return settings_; }

 private:
  Map settings_;
};

// -----------------------------------------------------------------------------
// Setting Key Constants
// -----------------------------------------------------------------------------
// Key constants for experimental features and settings consumed within the
// Chromium media layer. For platform-level Starboard features, see
// starboard/shared/starboard/experimental_features.h.
// keep-sorted start
inline constexpr ExperimentalFeatureKey<bool> kMediaBypassMojoForMedia(
    "Media.BypassMojoForMedia");
inline constexpr ExperimentalFeatureKey<bool> kMediaEnableTrivialOptimizations(
    "Media.EnableTrivialOptimizations");
inline constexpr ExperimentalFeatureKey<bool> kMediaForceClearSurfaceView(
    "Media.ForceClearSurfaceView");
inline constexpr ExperimentalFeatureKey<bool> kMediaForceDecodeToTexture(
    "Media.ForceDecodeToTexture");
inline constexpr ExperimentalFeatureKey<int> kMediaMaxSamplesPerWrite(
    "Media.MaxSamplesPerWrite");
// keep-sorted end

}  // namespace media

#endif  // MEDIA_BASE_STARBOARD_EXPERIMENTAL_FEATURES_H_
