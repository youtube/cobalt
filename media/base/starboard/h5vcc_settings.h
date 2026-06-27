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

#ifndef MEDIA_BASE_STARBOARD_H5VCC_SETTINGS_H_
#define MEDIA_BASE_STARBOARD_H5VCC_SETTINGS_H_

#include <cstdint>
#include <functional>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <type_traits>

#include "media/base/media_export.h"

namespace media {

using H5vccSettingsMap = std::map<std::string, int64_t, std::less<>>;

// -----------------------------------------------------------------------------
// H5vccSettingsKey Class
// -----------------------------------------------------------------------------
template <typename T>
class MEDIA_EXPORT H5vccSettingsKey {
 public:
  using ValueType = T;

  constexpr explicit H5vccSettingsKey(std::string_view key) : key_(key) {}

  constexpr std::string_view key() const { return key_; }

  // Returns the boolean value for the given key, falling back to false if the
  // key is missing or unset.
  bool GetBool(const H5vccSettingsMap& settings) const {
    static_assert(std::is_same_v<T, bool>, "GetBool called on non-boolean key");
    auto it = settings.find(key_);
    if (it == settings.end()) {
      return false;
    }
    return it->second != 0;
  }

  std::optional<T> Get(const H5vccSettingsMap& settings) const {
    auto it = settings.find(key_);
    if (it == settings.end()) {
      return std::nullopt;
    }
    if constexpr (std::is_same_v<T, bool>) {
      return it->second != 0;
    } else if constexpr (std::is_same_v<T, int>) {
      int64_t val = it->second;
      // Experiment framework uses 0 as the sentinel value for unset.
      // http://go/latestexpcl/player_web/features/player_web_cobalt.impl.gcl;l=332;rcl=862772714
      constexpr int kH5vccUnsetSentinel = 0;
      if (val == kH5vccUnsetSentinel) {
        return std::nullopt;
      }
      return static_cast<int>(val);
    } else {
      static_assert(sizeof(T) == 0,
                    "Unsupported type for H5vccSettingsKey::Get");
      return std::nullopt;
    }
  }

 private:
  std::string_view key_;
};

// -----------------------------------------------------------------------------
// Setting Key Constants
// -----------------------------------------------------------------------------
// keep-sorted start
inline constexpr H5vccSettingsKey<bool> kMediaBypassMojoForMedia(
    "Media.BypassMojoForMedia");
inline constexpr H5vccSettingsKey<bool> kMediaEnableTrivialOptimizations(
    "Media.EnableTrivialOptimizations");
inline constexpr H5vccSettingsKey<bool> kMediaForceClearSurfaceView(
    "Media.ForceClearSurfaceView");
inline constexpr H5vccSettingsKey<bool> kMediaForceDecodeToTexture(
    "Media.ForceDecodeToTexture");
inline constexpr H5vccSettingsKey<int> kMediaMaxSamplesPerWrite(
    "Media.MaxSamplesPerWrite");
// keep-sorted end

}  // namespace media

#endif  // MEDIA_BASE_STARBOARD_H5VCC_SETTINGS_H_
