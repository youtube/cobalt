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
  ExperimentalFeatures() = default;
  explicit ExperimentalFeatures(std::map<std::string, std::string> settings);
  ~ExperimentalFeatures() = default;

  bool GetBool(const ExperimentalFeatureKey& key) const;
  std::optional<bool> GetOptionalBool(const ExperimentalFeatureKey& key) const;
  std::optional<int> GetRangedInt(const ExperimentalFeatureKey& key,
                                  int min_val,
                                  int max_val) const;

  const std::map<std::string, std::string>& settings() const {
    return settings_;
  }

 private:
  std::map<std::string, std::string> settings_;
};

// Sets the experimental features for the current thread.
void SetExperimentalFeaturesForCurrentThread(
    const StarboardExtensionExperimentalFeatures* experimental_features);

// Gets the experimental features for the current thread.
const ExperimentalFeatures& GetExperimentalFeaturesForCurrentThread();

// Get the extension API for configuring experimental features.
const void* GetExperimentalFeaturesConfigurationApi();

}  // namespace starboard

#endif  // STARBOARD_SHARED_STARBOARD_EXPERIMENTAL_FEATURES_H_
