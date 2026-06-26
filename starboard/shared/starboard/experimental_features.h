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

#ifndef STARBOARD_SHARED_STARBOARD_EXPERIMENTAL_FEATURES_H_
#define STARBOARD_SHARED_STARBOARD_EXPERIMENTAL_FEATURES_H_

#include <cerrno>
#include <cstdlib>
#include <functional>
#include <map>
#include <optional>
#include <string>
#include <string_view>

#include "starboard/extension/experimental/experimental_features.h"

namespace starboard {

class ExperimentalFeatureKey {
 public:
  constexpr explicit ExperimentalFeatureKey(std::string_view key) : key_(key) {}
  constexpr std::string_view key() const { return key_; }

 private:
  std::string_view key_;
};

// keep-sorted start
inline constexpr ExperimentalFeatureKey kMediaAllowAudioWritingOnPause(
    "Media.AllowAudioWritingOnPause");
inline constexpr ExperimentalFeatureKey kMediaEnableAv1StartupOptimization(
    "Media.EnableAv1StartupOptimization");
inline constexpr ExperimentalFeatureKey kMediaEnableFlushDuringSeek(
    "Media.EnableFlushDuringSeek");
inline constexpr ExperimentalFeatureKey kMediaEnableLowLatency(
    "Media.EnableLowLatency");
inline constexpr ExperimentalFeatureKey kMediaEnableResetAudioDecoder(
    "Media.EnableResetAudioDecoder");
inline constexpr ExperimentalFeatureKey
    kMediaEnableSimdBasedAudioFormatSwitching(
        "Media.EnableSimdBasedAudioFormatSwitching");
inline constexpr ExperimentalFeatureKey kMediaEnableTrivialOptimizations(
    "Media.EnableTrivialOptimizations");
inline constexpr ExperimentalFeatureKey kMediaEnableVideoRendererVspAdjustment(
    "Media.EnableVideoRendererVspAdjustment");
inline constexpr ExperimentalFeatureKey kMediaFlushAudioTrackDuringSeek(
    "Media.FlushAudioTrackDuringSeek");
inline constexpr ExperimentalFeatureKey kMediaForceClearSurfaceView(
    "Media.ForceClearSurfaceView");
inline constexpr ExperimentalFeatureKey
    kMediaIgnoreMediaCodecCallbacksDuringFlushing(
        "Media.IgnoreMediaCodecCallbacksDuringFlushing");
inline constexpr ExperimentalFeatureKey kMediaSkipFlushOnDecoderTeardown(
    "Media.SkipFlushOnDecoderTeardown");
inline constexpr ExperimentalFeatureKey kMediaSkipVideoFramesOver60Fps(
    "Media.SkipVideoFramesOver60Fps");
inline constexpr ExperimentalFeatureKey kMediaVideoDecoderInitialPrerollCount(
    "Media.VideoDecoderInitialPrerollCount");
inline constexpr ExperimentalFeatureKey kMediaVideoRendererMinDecodedFrames(
    "Media.VideoRendererMinDecodedFrames");
inline constexpr ExperimentalFeatureKey kMediaVideoRendererMinInputBuffers(
    "Media.VideoRendererMinInputBuffers");
// keep-sorted end

class ExperimentalFeatures {
 public:
  using Map = std::map<std::string, std::string, std::less<>>;

  ExperimentalFeatures() = default;
  explicit ExperimentalFeatures(const Map& settings);
  ~ExperimentalFeatures() = default;

  bool GetBool(const ExperimentalFeatureKey& key) const;

  template <typename T>
  std::optional<T> Get(const ExperimentalFeatureKey& key) const {
    static_assert(sizeof(T) == 0,
                  "Unsupported type for ExperimentalFeatures::Get<T>");
    return std::nullopt;
  }

  const Map& settings() const { return settings_; }

 private:
  Map settings_;
};

template <>
inline std::optional<bool> ExperimentalFeatures::Get<bool>(
    const ExperimentalFeatureKey& key) const {
  auto it = settings().find(key.key());
  if (it == settings().end()) {
    return std::nullopt;
  }
  return it->second != "0" && it->second != "false";
}

template <>
inline std::optional<int> ExperimentalFeatures::Get<int>(
    const ExperimentalFeatureKey& key) const {
  auto it = settings().find(key.key());
  if (it == settings().end()) {
    return std::nullopt;
  }
  char* end = nullptr;
  errno = 0;
  long val = strtol(it->second.c_str(), &end, /*base=*/10);
  if (errno == ERANGE || end == it->second.c_str() || *end != '\0') {
    return std::nullopt;
  }
  return static_cast<int>(val);
}

// Sets the experimental features for the current thread.
void SetExperimentalFeaturesForCurrentThread(
    const StarboardExtensionExperimentalFeatures* experimental_features);

// Gets the experimental features for the current thread.
const ExperimentalFeatures& GetExperimentalFeaturesForCurrentThread();

// Get the extension API for configuring experimental features.
const void* GetExperimentalFeaturesConfigurationApi();

}  // namespace starboard

#endif  // STARBOARD_SHARED_STARBOARD_EXPERIMENTAL_FEATURES_H_
