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

#ifndef STARBOARD_EXTENSION_EXPERIMENTAL_FEATURES_H_
#define STARBOARD_EXTENSION_EXPERIMENTAL_FEATURES_H_

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define kStarboardExtensionExperimentalFeaturesConfigurationName \
  "dev.starboard.extension.ExperimentalFeaturesConfiguration"

// This extension is intended ONLY for temporary experiments. Once an experiment
// is concluded and the feature is launched, the corresponding field should be
// removed from this struct and moved to a dedicated extension or a permanent
// Starboard function.
typedef struct StarboardExtensionExperimentalFeatures {
  // If a field is NULL, it means the value is not set.
  // The fields should be in alphabetical order.
  bool disable_low_performance_sw_decoder;
  bool enable_av1_startup_optimization;
  bool enable_codec_output_checker;
  bool flush_decoder_during_reset;
  bool reset_audio_decoder;
  bool skip_flush_on_decoder_teardown;
  const int* video_decoder_initial_preroll_count;
  const int* video_renderer_min_decoded_frames;
  const int* video_renderer_min_input_buffers;
  const bool* use_dual_threads_for_video;
} StarboardExtensionExperimentalFeatures;

typedef struct StarboardExtensionExperimentalFeaturesConfigurationApi {
  // Name should be the string
  // |kStarboardExtensionExperimentalFeaturesConfigurationName|. This helps to
  // validate that the extension API is correct.
  const char* name;

  // This specifies the version of the API that is implemented.
  uint32_t version;

  // The fields below this point were added in version 1 or later.

  // Sets the experimental features for the current thread.
  void (*SetExperimentalFeaturesForCurrentThread)(
      const StarboardExtensionExperimentalFeatures* experimental_features);
} StarboardExtensionExperimentalFeaturesConfigurationApi;

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // STARBOARD_EXTENSION_EXPERIMENTAL_FEATURES_H_
