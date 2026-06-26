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

#include "media/base/starboard/h5vcc_settings.h"

#include "base/strings/string_number_conversions.h"

namespace media {

namespace {
constexpr int kH5vccUnsetSentinel = 0;
}  // namespace

bool H5vccSettingsKey::GetBool(const H5vccSettingsMap& settings) const {
  auto it = settings.find(key_);
  if (it == settings.end()) {
    return false;
  }
  return it->second != "0" && it->second != "false";
}

std::optional<bool> H5vccSettingsKey::GetOptionalBool(
    const H5vccSettingsMap& settings) const {
  auto it = settings.find(key_);
  if (it == settings.end()) {
    return std::nullopt;
  }
  return it->second != "0" && it->second != "false";
}

std::optional<int> H5vccSettingsKey::GetRangedInt(
    const H5vccSettingsMap& settings,
    int min_val,
    int max_val) const {
  return GetRangedInt(settings, min_val, max_val, kH5vccUnsetSentinel);
}

std::optional<int> H5vccSettingsKey::GetRangedInt(
    const H5vccSettingsMap& settings,
    int min_val,
    int max_val,
    int unset_sentinel) const {
  auto it = settings.find(key_);
  if (it == settings.end()) {
    return std::nullopt;
  }
  int64_t val = 0;
  if (!base::StringToInt64(it->second, &val)) {
    return std::nullopt;
  }
  if (val == unset_sentinel || val < min_val || val > max_val) {
    return std::nullopt;
  }
  return static_cast<int>(val);
}

}  // namespace media
