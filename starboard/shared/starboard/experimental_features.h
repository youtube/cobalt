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

#include <cstdint>
#include <functional>
#include <limits>
#include <map>
#include <optional>
#include <ostream>
#include <string>
#include <string_view>
#include <type_traits>
#include <variant>

#include "starboard/extension/experimental/experimental_features.h"

namespace starboard {
namespace internal {

// Experiment framework uses 0 as the sentinel value for unset.
// e.g.,
// http://go/latestexpcl/player_web/features/player_web_cobalt.impl.gcl;l=332;rcl=862772714
constexpr int kH5vccUnsetSentinel = 0;

}  // namespace internal

// Strongly typed identifier for an experimental feature key used within
// Starboard.
//
// Threading & Lifetime: Lightweight constexpr wrappers around a compile-time
// string_view. Thread-safe and intended to be used as static constants.
template <typename T>
class ExperimentalFeatureKey {
 public:
  using ValueType = T;
  using DefaultValueType =
      std::conditional_t<std::is_same_v<T, std::string>, const char*, T>;

  template <size_t N>
  constexpr explicit ExperimentalFeatureKey(const char (&key)[N])
      : key_(key, N - 1), default_value_(std::nullopt) {}

  template <size_t N>
  constexpr explicit ExperimentalFeatureKey(const char (&key)[N],
                                            DefaultValueType default_value)
      : key_(key, N - 1), default_value_(default_value) {}

  constexpr std::string_view key() const { return key_; }
  constexpr const std::optional<DefaultValueType>& default_value() const {
    return default_value_;
  }

 private:
  std::string_view key_;
  std::optional<DefaultValueType> default_value_;
};

// Container for experimental feature settings stored per-thread in Starboard.
//
// Threading & Lifetime: Managed per thread via function-scoped thread-local
// storage. Instances are copyable and owned by the thread-local accessor.
class ExperimentalFeatures {
 public:
  using Value = std::variant<int64_t, std::string>;
  using Map = std::map<std::string, Value, std::less<>>;

  ExperimentalFeatures() = default;
  explicit ExperimentalFeatures(Map settings);
  ~ExperimentalFeatures() = default;

  // Returns the boolean value for the given key, falling back to false if the
  // key is missing or is unset with no default value.
  bool GetBool(const ExperimentalFeatureKey<bool>& key) const {
    return Get(key).value_or(false);
  }

  template <typename T>
  std::optional<T> Get(const ExperimentalFeatureKey<T>& key) const {
    auto it = settings_.find(key.key());
    if (it != settings_.end()) {
      if (auto val = GetValue<T>(it->second); val.has_value()) {
        return val;
      }
    }
    if (key.default_value().has_value()) {
      return T(*key.default_value());
    }
    return std::nullopt;
  }

  friend std::ostream& operator<<(std::ostream& os,
                                  const ExperimentalFeatures& features);

 private:
  template <typename T>
  std::optional<T> GetValue(const Value& val) const;

  Map settings_;
};

template <typename T>
inline std::optional<T> ExperimentalFeatures::GetValue(const Value& val) const {
  static_assert(!std::is_same_v<T, T>,
                "Unsupported type for ExperimentalFeatures::Get");
  return std::nullopt;
}

template <>
inline std::optional<bool> ExperimentalFeatures::GetValue<bool>(
    const Value& val) const {
  auto* int_val = std::get_if<int64_t>(&val);
  if (!int_val) {
    return std::nullopt;
  }
  return *int_val != 0;
}

template <>
inline std::optional<int> ExperimentalFeatures::GetValue<int>(
    const Value& val) const {
  auto* int_val = std::get_if<int64_t>(&val);
  if (!int_val || *int_val == internal::kH5vccUnsetSentinel) {
    return std::nullopt;
  }
  if (*int_val < std::numeric_limits<int>::min() ||
      *int_val > std::numeric_limits<int>::max()) {
    return std::nullopt;
  }
  return static_cast<int>(*int_val);
}

template <>
inline std::optional<std::string> ExperimentalFeatures::GetValue<std::string>(
    const Value& val) const {
  auto* str_val = std::get_if<std::string>(&val);
  if (!str_val) {
    return std::nullopt;
  }
  return *str_val;
}

// Sets the experimental features for the current thread.
void SetExperimentalFeaturesForCurrentThread(
    const StarboardExtensionExperimentalFeatures* experimental_features);

// Gets the experimental features for the current thread.
const ExperimentalFeatures& GetExperimentalFeaturesForCurrentThread();

// Get the extension API for configuring experimental features.
const void* GetExperimentalFeaturesConfigurationApi();

// -----------------------------------------------------------------------------
// Experimental Feature Key Constants
// -----------------------------------------------------------------------------
// Key constants for experimental features consumed directly within the
// Starboard platform implementation layer. For Chromium media layer settings,
// see media/base/starboard/experimental_features.h.
// keep-sorted start by_regex=k\w+ newline_separated=yes
// Allows writing audio samples while paused to reduce resume latency.
// Feature bug: b/500811542
// Experiment bug: b/512923901
inline constexpr ExperimentalFeatureKey<bool> kMediaAllowAudioWritingOnPause(
    "Media.AllowAudioWritingOnPause");

// Enables AV1 video startup latency optimizations on Android.
// Feature bug: b/486980027
inline constexpr ExperimentalFeatureKey<bool>
    kMediaEnableAv1StartupOptimization("Media.EnableAv1StartupOptimization");

// Dynamically enables flushing decoders during seek operations.
// Feature bug: b/474454335
inline constexpr ExperimentalFeatureKey<bool> kMediaEnableFlushDuringSeek(
    "Media.EnableFlushDuringSeek");

// Enables resetting the audio decoder during seek operations.
// Feature bug: b/474454335
inline constexpr ExperimentalFeatureKey<bool> kMediaEnableResetAudioDecoder(
    "Media.EnableResetAudioDecoder");

// Enables SIMD-accelerated audio sample format conversion in `DecodedAudio`.
// Feature bug: b/518861272
inline constexpr ExperimentalFeatureKey<bool>
    kMediaEnableSimdBasedAudioFormatSwitching(
        "Media.EnableSimdBasedAudioFormatSwitching");

// Enables steady-state playback optimizations in `VideoRendererImpl` (e.g.,
// pre-checking seek state before atomic ops, splicing decoded frame lists).
// Feature bug: b/514758473
inline constexpr ExperimentalFeatureKey<bool> kMediaEnableTrivialOptimizations(
    "Media.EnableTrivialOptimizations");

// Enables Video Synchronization Point (VSP) timestamp adjustment in the video
// renderer to smooth frame pacing on Android.
// Feature bug: b/311422213, b/454305379
inline constexpr ExperimentalFeatureKey<bool>
    kMediaEnableVideoRendererVspAdjustment(
        "Media.EnableVideoRendererVspAdjustment");

// Prevents video pipeline backpressure leak on Android where pending frames
// grow unbounded (2000+).
// Feature bug: b/515102461, b/517914191
// Experiment bug: b/539672039
inline constexpr ExperimentalFeatureKey<bool>
    kMediaFixNeedMoreInputBackpressure("Media.FixNeedMoreInputBackpressure");

// Forces flushing the Android `AudioTrack` during seek or reset operations.
// Feature bug: b/330793785
inline constexpr ExperimentalFeatureKey<bool> kMediaFlushAudioTrackDuringSeek(
    "Media.FlushAudioTrackDuringSeek");

// Forces dual threads (`VidDecIn` and `VidDecOut`) for video decoding on
// multi-core platforms.
// Feature bug: b/329686979
inline constexpr ExperimentalFeatureKey<bool> kMediaForceDualThreads(
    "Media.ForceDualThreads");

// Forces the use of software video decoders instead of hardware decoders.
// Feature bug: b/545881570, b/490474392
inline constexpr ExperimentalFeatureKey<bool> kMediaForceSoftwareVideoDecoder(
    "Media.ForceSoftwareVideoDecoder");

// Discards stale `MediaCodec` callbacks queued before or during `flush()` to
// prevent inconsistent decoder state.
// Feature bug: b/497004556
// Experiment bug: b/524642946
inline constexpr ExperimentalFeatureKey<bool>
    kMediaIgnoreMediaCodecCallbacksDuringFlushing(
        "Media.IgnoreMediaCodecCallbacksDuringFlushing",
        true);

// Ignores stale rendered frame callbacks after a seek operation before the
// first post-seek frame is processed, preventing false-positive dropped frame
// counts.
// Feature bug: b/401790323, b/534774024
inline constexpr ExperimentalFeatureKey<bool>
    kMediaIgnoreStaleRenderedFramesAfterSeek(
        "Media.IgnoreStaleRenderedFramesAfterSeek");

// Uses Android NDK-based `AAudio` / `AudioTrack` instead of Java `AudioTrack`
// to reduce JNI overhead.
// Feature bug: b/428008986
// Experiment bug: b/543131997
inline constexpr ExperimentalFeatureKey<bool> kMediaNdkAudioTrack(
    "Media.NdkAudioTrack");

// Uses Android NDK `AMediaCodec` APIs (`NdkMediaCodec`) instead of Java
// `MediaCodec` for non-secure, non-tunneled video decoding to reduce JNI
// overhead.
// Feature bug: b/515461431, b/531404834
inline constexpr ExperimentalFeatureKey<bool> kMediaNdkVideo("Media.NdkVideo");

// Uses `AudioTrack` state when pausing playback to prevent media time drift
// upon resume.
// Feature bug: b/349854301, b/544962204
inline constexpr ExperimentalFeatureKey<bool> kMediaPauseUsingAudioTrackState(
    "Media.PauseUsingAudioTrackState");

// Enables seamless PCM audio device transitions (e.g. Bluetooth A2DP connect
// or disconnect) without tearing down and recreating the player pipeline.
// Feature bug: b/523148108
// Experiment bug: b/556764997
inline constexpr ExperimentalFeatureKey<bool> kMediaSeamlessAudioSwitching(
    "Media.SeamlessAudioSwitching");

// Skips internal `MediaCodec.flush()` when tearing down the video decoder to
// reduce player destruction overhead.
// Feature bug: b/492971394
inline constexpr ExperimentalFeatureKey<bool> kMediaSkipFlushOnDecoderTeardown(
    "Media.SkipFlushOnDecoderTeardown");

// Skips frames exceeding 60fps to work around video stuttering on Android
// devices with high operating rates.
// Feature bug: b/506257255
inline constexpr ExperimentalFeatureKey<bool> kMediaSkipVideoFramesOver60Fps(
    "Media.SkipVideoFramesOver60Fps");

// Customizes the initial number of video frames the decoder buffers (prerolls)
// before starting playback.
// Feature bug: b/487097162
inline constexpr ExperimentalFeatureKey<int>
    kMediaVideoDecoderInitialPrerollCount(
        "Media.VideoDecoderInitialPrerollCount");

// Controls the maximum number of pending input buffers allowed in the video
// decoder queue (default: 128).
// Feature bug: b/517914191
// Experiment bug: b/539672039
inline constexpr ExperimentalFeatureKey<int>
    kMediaVideoDecoderMaxPendingInputsSize(
        "Media.VideoDecoderMaxPendingInputsSize");

// Controls the minimum number of decoded video frames required in
// `VideoRendererImpl` before playback starts.
// Feature bug: b/485225923
// Experiment bug: b/491104896
inline constexpr ExperimentalFeatureKey<int>
    kMediaVideoRendererMinDecodedFrames("Media.VideoRendererMinDecodedFrames");

// Controls the minimum number of input buffers required in `VideoRendererImpl`
// before playback starts.
// Feature bug: b/485225923
// Experiment bug: b/491104896
inline constexpr ExperimentalFeatureKey<int> kMediaVideoRendererMinInputBuffers(
    "Media.VideoRendererMinInputBuffers");
// keep-sorted end

}  // namespace starboard

#endif  // STARBOARD_SHARED_STARBOARD_EXPERIMENTAL_FEATURES_H_
