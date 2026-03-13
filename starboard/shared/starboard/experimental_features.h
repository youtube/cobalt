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

#include <optional>

#include "starboard/extension/experimental_features.h"

namespace starboard::shared::starboard {

// Internal C++ representation of temporary experimental features.
// Once an experiment is concluded and the feature is launched, the
// corresponding field should be removed from this struct and moved to its own
// dedicated function.
struct ExperimentalFeatures {
  // The fields should be in alphabetical order.
  bool flush_decoder_during_reset = false;
  std::optional<int> media_codec_reset_delay_ms;
  bool pause_using_audio_track_state = false;
  bool reset_audio_decoder = false;
  std::optional<int> video_decoder_initial_preroll_count;
  std::optional<int> video_decoder_poll_interval_ms;
  std::optional<int> video_initial_max_frames_in_decoder;
  std::optional<int> video_max_pending_input_frames;
  std::optional<int> video_renderer_min_decoded_frames;
  std::optional<int> video_renderer_min_input_buffers;
};

// Sets the experimental features for the current thread.
void SetExperimentalFeaturesForCurrentThread(
    const StarboardExtensionExperimentalFeatures* experimental_features);

// Gets the experimental features for the current thread.
const ExperimentalFeatures& GetExperimentalFeaturesForCurrentThread();

// Get the extension API for configuring experimental features.
const void* GetExperimentalFeaturesConfigurationApi();

}  // namespace starboard::shared::starboard

#endif  // STARBOARD_SHARED_STARBOARD_EXPERIMENTAL_FEATURES_H_
