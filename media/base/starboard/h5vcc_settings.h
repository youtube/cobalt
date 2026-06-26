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

  std::optional<bool> GetOptionalBool(const H5vccSettingsMap& settings) const;

  std::optional<int> GetRangedInt(const H5vccSettingsMap& settings,
                                  int min_val,
                                  int max_val) const;

  std::optional<int> GetRangedInt(const H5vccSettingsMap& settings,
                                  int min_val,
                                  int max_val,
                                  int unset_sentinel) const;

 private:
  std::string_view key_;
};

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
