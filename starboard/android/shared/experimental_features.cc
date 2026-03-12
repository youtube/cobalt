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

#include "starboard/android/shared/experimental_features.h"

#include <optional>

#include "starboard/android/shared/experimental_features_internal.h"
#include "starboard/common/log.h"
#include "starboard/extension/experimental_features.h"

namespace starboard::android::shared {

namespace {

using ::starboard::shared::starboard::player::filter::ExperimentalFeatures;

std::optional<int> FromIntPointer(const int* val) {
  if (!val) {
    return std::nullopt;
  }
  return *val;
}

void SetExperimentalFeaturesForCurrentThreadAsAdapter(
    const StarboardExtensionExperimentalFeatures* extension_features) {
  // |extension_features| cannot be null. We use a pointer here to support C API
  // compatibility.
  SB_CHECK(extension_features);

  ExperimentalFeatures internal_features;

  internal_features.media_codec_reset_delay_ms =
      FromIntPointer(extension_features->media_codec_reset_delay_ms);
  internal_features.video_decoder_initial_preroll_count =
      FromIntPointer(extension_features->video_decoder_initial_preroll_count);
  internal_features.video_decoder_poll_interval_ms =
      FromIntPointer(extension_features->video_decoder_poll_interval_ms);
  internal_features.video_initial_max_frames_in_decoder =
      FromIntPointer(extension_features->video_initial_max_frames_in_decoder);
  internal_features.video_max_pending_input_frames =
      FromIntPointer(extension_features->video_max_pending_input_frames);
  internal_features.video_renderer_min_decoded_frames =
      FromIntPointer(extension_features->video_renderer_min_decoded_frames);
  internal_features.video_renderer_min_input_buffers =
      FromIntPointer(extension_features->video_renderer_min_input_buffers);

  SetExperimentalFeaturesForCurrentThread(internal_features);
}

const StarboardExtensionExperimentalFeaturesConfigurationApi
    kExperimentalFeaturesConfigurationApi = {
        kStarboardExtensionExperimentalFeaturesConfigurationName,
        1,
        &SetExperimentalFeaturesForCurrentThreadAsAdapter,
};

}  // namespace

const void* GetExperimentalFeaturesConfigurationApi() {
  return &kExperimentalFeaturesConfigurationApi;
}

}  // namespace starboard::android::shared
