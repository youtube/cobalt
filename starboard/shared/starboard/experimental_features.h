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

#ifndef STARBOARD_SHARED_STARBOARD_EXPERIMENTAL_FEATURES_H_
#define STARBOARD_SHARED_STARBOARD_EXPERIMENTAL_FEATURES_H_

#include <cerrno>
#include <cstdint>
#include <cstdlib>
#include <functional>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <type_traits>
#include <variant>

#include "starboard/extension/experimental/experimental_features.h"

namespace starboard {

// Strongly typed identifier for an experimental feature key used within
// Starboard.
//
// Threading & Lifetime: Lightweight constexpr wrappers around a compile-time
// string_view. Thread-safe and intended to be used as static constants.
template <typename T>
class ExperimentalFeatureKey {
 public:
  using ValueType = T;
  constexpr explicit ExperimentalFeatureKey(std::string_view key) : key_(key) {}
  constexpr std::string_view key() const { return key_; }

 private:
  std::string_view key_;
};

// -----------------------------------------------------------------------------
// Experimental Feature Key Constants
// -----------------------------------------------------------------------------
// Key constants for experimental features consumed directly within the
// Starboard platform implementation layer. For Chromium media layer settings,
// see media/base/starboard/experimental_features.h. keep-sorted start
inline constexpr ExperimentalFeatureKey<bool> kMediaAllowAudioWritingOnPause(
    "Media.AllowAudioWritingOnPause");
inline constexpr ExperimentalFeatureKey<bool> kMediaEnableFlushDuringSeek(
    "Media.EnableFlushDuringSeek");
inline constexpr ExperimentalFeatureKey<bool> kMediaEnableLowLatency(
    "Media.EnableLowLatency");
inline constexpr ExperimentalFeatureKey<bool> kMediaEnableResetAudioDecoder(
    "Media.EnableResetAudioDecoder");
inline constexpr ExperimentalFeatureKey<bool> kMediaEnableTrivialOptimizations(
    "Media.EnableTrivialOptimizations");
inline constexpr ExperimentalFeatureKey<bool> kMediaFlushAudioTrackDuringSeek(
    "Media.FlushAudioTrackDuringSeek");
inline constexpr ExperimentalFeatureKey<bool> kMediaForceClearSurfaceView(
    "Media.ForceClearSurfaceView");
inline constexpr ExperimentalFeatureKey<bool> kMediaSkipFlushOnDecoderTeardown(
    "Media.SkipFlushOnDecoderTeardown");
inline constexpr ExperimentalFeatureKey<bool> kMediaSkipVideoFramesOver60Fps(
    "Media.SkipVideoFramesOver60Fps");
inline constexpr ExperimentalFeatureKey<bool>
    kMediaEnableAv1StartupOptimization("Media.EnableAv1StartupOptimization");
inline constexpr ExperimentalFeatureKey<bool>
    kMediaEnableSimdBasedAudioFormatSwitching(
        "Media.EnableSimdBasedAudioFormatSwitching");
inline constexpr ExperimentalFeatureKey<bool>
    kMediaEnableVideoRendererVspAdjustment(
        "Media.EnableVideoRendererVspAdjustment");
inline constexpr ExperimentalFeatureKey<bool>
    kMediaIgnoreMediaCodecCallbacksDuringFlushing(
        "Media.IgnoreMediaCodecCallbacksDuringFlushing");
inline constexpr ExperimentalFeatureKey<int> kMediaVideoRendererMinInputBuffers(
    "Media.VideoRendererMinInputBuffers");
inline constexpr ExperimentalFeatureKey<int>
    kMediaVideoDecoderInitialPrerollCount(
        "Media.VideoDecoderInitialPrerollCount");
inline constexpr ExperimentalFeatureKey<int>
    kMediaVideoRendererMinDecodedFrames("Media.VideoRendererMinDecodedFrames");
// keep-sorted end

// Container for experimental feature settings stored per-thread in Starboard.
//
// Threading & Lifetime: Managed per thread via function-scoped thread-local
// storage. Instances are copyable and owned by the thread-local accessor.
class ExperimentalFeatures {
 public:
  using Value = std::variant<int64_t, std::string>;
  using Map = std::map<std::string, Value, std::less<>>;

  ExperimentalFeatures() = default;
  explicit ExperimentalFeatures(const Map& settings);
  ~ExperimentalFeatures() = default;

  // Returns the boolean value for the given key, falling back to false if the
  // key is missing or unset.
  bool GetBool(const ExperimentalFeatureKey<bool>& key) const;

  template <typename T>
  std::optional<T> Get(const ExperimentalFeatureKey<T>& key) const {
    auto it = settings().find(key.key());
    if (it == settings().end()) {
      return std::nullopt;
    }
    return GetValue<T>(it->second);
  }

  const Map& settings() const { return settings_; }

 private:
  template <typename T>
  std::optional<T> GetValue(const Value& val) const;

  Map settings_;
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
