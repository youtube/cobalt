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

#ifndef STARBOARD_EXTENSION_VIDEO_DECODER_CONFIGURATION_H_
#define STARBOARD_EXTENSION_VIDEO_DECODER_CONFIGURATION_H_

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define kStarboardExtensionVideoDecoderConfigurationName \
  "dev.starboard.extension.VideoDecoderConfiguration"

typedef struct OptionalInt {
  bool is_set;
  int value;
} OptionalInt;

typedef struct StarboardVideoDecoderExperimentalFeatures {
  OptionalInt initial_max_frames_in_decoder;
  OptionalInt max_pending_input_frames;
  OptionalInt video_decoder_poll_interval_ms;
} StarboardVideoDecoderExperimentalFeatures;

typedef struct StarboardExtensionVideoDecoderConfigurationApi {
  // Name should be the string
  // |kStarboardExtensionVideoDecoderConfigurationName|. This helps to validate
  // that the extension API is correct.
  const char* name;

  // This specifies the version of the API that is implemented.
  uint32_t version;

  // Specifies the video poll interval in milliseconds.
  void (*SetExperimentalFeaturesForCurrentThread)(
      const StarboardVideoDecoderExperimentalFeatures* experimental_features);
} StarboardExtensionVideoDecoderConfigurationApi;

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // STARBOARD_EXTENSION_VIDEO_DECODER_CONFIGURATION_H_
