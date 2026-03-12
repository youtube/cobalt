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

typedef struct StarboardExtensionExperimentalFeatures {
  // The fields below are optional. If they are NULL, it means the value is not
  // set.
  const int* media_codec_reset_delay_ms;
  const int* video_decoder_initial_preroll_count;
  const int* video_decoder_poll_interval_ms;
  const int* video_initial_max_frames_in_decoder;
  const int* video_max_pending_input_frames;
  const int* video_renderer_min_decoded_frames;
  const int* video_renderer_min_input_buffers;
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
