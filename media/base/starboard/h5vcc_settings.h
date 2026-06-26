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

#include <functional>
#include <map>
#include <optional>
#include <string>
#include <string_view>

#include "base/strings/string_number_conversions.h"
#include "media/base/media_export.h"

namespace media {

using H5vccSettingsMap = std::map<std::string, std::string, std::less<>>;

// -----------------------------------------------------------------------------
// H5vccSettingsKey Class
// -----------------------------------------------------------------------------
class MEDIA_EXPORT H5vccSettingsKey {
 public:
  constexpr explicit H5vccSettingsKey(std::string_view key) : key_(key) {}

  constexpr std::string_view key() const { return key_; }

  bool GetBool(const H5vccSettingsMap& settings) const;

  template <typename T>
  std::optional<T> Get(const H5vccSettingsMap& settings) const {
    static_assert(sizeof(T) == 0,
                  "Unsupported type for H5vccSettingsKey::Get<T>");
    return std::nullopt;
  }

 private:
  std::string_view key_;
};

template <>
inline std::optional<bool> H5vccSettingsKey::Get<bool>(
    const H5vccSettingsMap& settings) const {
  auto it = settings.find(key_);
  if (it == settings.end()) {
    return std::nullopt;
  }
  return it->second != "0" && it->second != "false";
}

template <>
inline std::optional<int> H5vccSettingsKey::Get<int>(
    const H5vccSettingsMap& settings) const {
  auto it = settings.find(key_);
  if (it == settings.end()) {
    return std::nullopt;
  }
  int64_t val = 0;
  if (!base::StringToInt64(it->second, &val)) {
    return std::nullopt;
  }
  if (val == 0) {
    return std::nullopt;
  }
  return static_cast<int>(val);
}

// -----------------------------------------------------------------------------
// Setting Key Constants
// -----------------------------------------------------------------------------
// keep-sorted start
inline constexpr H5vccSettingsKey kMediaBypassMojoForMedia(
    "Media.BypassMojoForMedia");
inline constexpr H5vccSettingsKey kMediaEnableTrivialOptimizations(
    "Media.EnableTrivialOptimizations");
inline constexpr H5vccSettingsKey kMediaForceClearSurfaceView(
    "Media.ForceClearSurfaceView");
inline constexpr H5vccSettingsKey kMediaForceDecodeToTexture(
    "Media.ForceDecodeToTexture");
inline constexpr H5vccSettingsKey kMediaMaxSamplesPerWrite(
    "Media.MaxSamplesPerWrite");
// keep-sorted end

}  // namespace media

#endif  // MEDIA_BASE_STARBOARD_H5VCC_SETTINGS_H_
