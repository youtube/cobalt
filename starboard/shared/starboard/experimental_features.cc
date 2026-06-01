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

#include <atomic>
#include <optional>
#include <type_traits>

#include "starboard/common/log.h"
#include "starboard/extension/experimental/experimental_features.h"

namespace starboard {

namespace {

// A thread-safe wrapper around std::optional<bool> using std::atomic.
// This class is used to store global experimental settings that can be
// concurrently read from multiple threads (e.g., audio decoder threads)
// and written during initialization.
//
// Lifetime: Matches the global static variable it is used for.
// Threading Model: Thread-safe, can be read and written from any thread.
class AtomicOptionalBool {
 public:
  constexpr AtomicOptionalBool() : state_(OptionalBoolState::kUnset) {}
  explicit AtomicOptionalBool(std::optional<bool> value)
      : state_(OptionalBoolState::kUnset) {
    *this = value;
  }

  AtomicOptionalBool(const AtomicOptionalBool&) = delete;
  AtomicOptionalBool& operator=(const AtomicOptionalBool&) = delete;

  void operator=(std::optional<bool> value) {
    OptionalBoolState state_val = OptionalBoolState::kUnset;
    if (value.has_value()) {
      state_val =
          value.value() ? OptionalBoolState::kTrue : OptionalBoolState::kFalse;
    }
    state_.store(state_val, std::memory_order_release);
  }

  operator std::optional<bool>() const {
    OptionalBoolState val = state_.load(std::memory_order_acquire);
    if (val == OptionalBoolState::kUnset) {
      return std::nullopt;
    }
    return val == OptionalBoolState::kTrue;
  }

 private:
  enum class OptionalBoolState : int {
    kUnset = -1,
    kFalse = 0,
    kTrue = 1,
  };

  std::atomic<OptionalBoolState> state_;
};

static_assert(
    std::is_trivially_destructible<std::optional<ExperimentalFeatures>>::value,
    "g_experimental_features must be trivially destructible.");
static_assert(sizeof(ExperimentalFeatures) < 256,
              "ExperimentalFeatures is too large for thread-local storage.");
thread_local std::optional<ExperimentalFeatures> g_experimental_features;

static_assert(std::is_trivially_destructible<AtomicOptionalBool>::value,
              "g_enable_simd_based_audio_format_switching must be trivially "
              "destructible.");
AtomicOptionalBool g_enable_simd_based_audio_format_switching;

std::optional<int> FromIntPointer(const int* val) {
  if (!val) {
    return std::nullopt;
  }
  return *val;
}

std::optional<bool> FromBoolPointer(const bool* val) {
  if (!val) {
    return std::nullopt;
  }
  return *val;
}

const StarboardExtensionExperimentalFeaturesConfigurationApi
    kExperimentalFeaturesConfigurationApi = {
        kStarboardExtensionExperimentalFeaturesConfigurationName,
        1,
        &SetExperimentalFeaturesForCurrentThread,
};

}  // namespace

void SetExperimentalFeaturesForCurrentThread(
    const StarboardExtensionExperimentalFeatures* extension_features) {
  // |extension_features| cannot be null. We use a pointer here to support C API
  // compatibility.
  SB_CHECK(extension_features);

  ExperimentalFeatures experiment_features;

  experiment_features.allow_audio_writing_on_pause =
      extension_features->allow_audio_writing_on_pause;
  experiment_features.disable_low_performance_sw_decoder =
      extension_features->disable_low_performance_sw_decoder;
  experiment_features.enable_av1_startup_optimization =
      extension_features->enable_av1_startup_optimization;
  experiment_features.enable_low_latency =
      extension_features->enable_low_latency;
  experiment_features.enable_video_renderer_vsp_adjustment =
      extension_features->enable_video_renderer_vsp_adjustment;
  experiment_features.flush_audio_track_during_seek =
      extension_features->flush_audio_track_during_seek;
  experiment_features.flush_decoder_during_reset =
      extension_features->flush_decoder_during_reset;
  experiment_features.ignore_mediacodec_callbacks_during_flushing =
      extension_features->ignore_mediacodec_callbacks_during_flushing;
  experiment_features.reset_audio_decoder =
      extension_features->reset_audio_decoder;
  experiment_features.skip_flush_on_decoder_teardown =
      extension_features->skip_flush_on_decoder_teardown;
  experiment_features.skip_video_frames_over_60_fps =
      extension_features->skip_video_frames_over_60_fps;
  experiment_features.enable_simd_based_audio_format_switching =
      FromBoolPointer(
          extension_features->enable_simd_based_audio_format_switching);
  experiment_features.enable_trivial_optimizations =
      FromBoolPointer(extension_features->enable_trivial_optimizations);
  experiment_features.use_dual_threads_for_video =
      FromBoolPointer(extension_features->use_dual_threads_for_video);
  experiment_features.video_decoder_initial_preroll_count =
      FromIntPointer(extension_features->video_decoder_initial_preroll_count);
  experiment_features.video_renderer_min_decoded_frames =
      FromIntPointer(extension_features->video_renderer_min_decoded_frames);
  experiment_features.video_renderer_min_input_buffers =
      FromIntPointer(extension_features->video_renderer_min_input_buffers);

  g_experimental_features = experiment_features;

  g_enable_simd_based_audio_format_switching =
      experiment_features.enable_simd_based_audio_format_switching;
}

const ExperimentalFeatures& GetExperimentalFeaturesForCurrentThread() {
  SB_CHECK(g_experimental_features.has_value())
      << "ExperimentalFeatures are not set. This method was likely "
         "called on the wrong thread or a race condition occurred.";
  return *g_experimental_features;
}

const void* GetExperimentalFeaturesConfigurationApi() {
  return &kExperimentalFeaturesConfigurationApi;
}

std::optional<bool> GetSimdBasedAudioFormatSwitchingSetting() {
  return g_enable_simd_based_audio_format_switching;
}

}  // namespace starboard
