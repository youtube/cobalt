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

#ifndef STARBOARD_ANDROID_SHARED_VIDEO_DECODER_CONFIGURATION_INTERNAL_H_
#define STARBOARD_ANDROID_SHARED_VIDEO_DECODER_CONFIGURATION_INTERNAL_H_

#include <optional>

#include "starboard/extension/video_decoder_configuration.h"

namespace starboard::android::shared {

struct VideoDecoderExperimentalFeatures {
  std::optional<int> initial_max_frames_in_decoder;
  std::optional<int> max_pending_input_frames;
  std::optional<int> video_decoder_poll_interval_ms;
};

// Get experimental features via SetExperimentalFeaturesForCurrentThread().
VideoDecoderExperimentalFeatures GetExperimentalFeaturesForCurrentThread();

// Specifies the experimental features for the video decoder on the current
// thread.
void SetExperimentalFeaturesForCurrentThread(
    const StarboardVideoDecoderExperimentalFeatures* experimental_features);

}  // namespace starboard::android::shared

#endif  // STARBOARD_ANDROID_SHARED_VIDEO_DECODER_CONFIGURATION_INTERNAL_H_
