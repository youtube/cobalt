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

#include <memory>
#include <optional>

#include "base/no_destructor.h"
#include "base/threading/thread_local.h"
#include "starboard/common/log.h"
#include "starboard/extension/experimental_features.h"

namespace starboard::shared::starboard {

namespace {

base::ThreadLocalOwnedPointer<ExperimentalFeatures>&
GetExperimentalFeaturesTLS() {
  static base::NoDestructor<base::ThreadLocalOwnedPointer<ExperimentalFeatures>>
      experimental_features_tls;
  return *experimental_features_tls;
}

ExperimentalFeatures* GetOrCreateExperimentalFeatures() {
  auto& tls = GetExperimentalFeaturesTLS();
  ExperimentalFeatures* ptr = tls.Get();
  if (ptr) {
    return ptr;
  }

  tls.Set(std::make_unique<ExperimentalFeatures>());
  return tls.Get();
}

std::optional<int> FromIntPointer(const int* val) {
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

  experiment_features.flush_decoder_during_reset =
      extension_features->flush_decoder_during_reset;
  experiment_features.media_codec_reset_delay_ms =
      FromIntPointer(extension_features->media_codec_reset_delay_ms);
  experiment_features.pause_using_audio_track_state =
      extension_features->pause_using_audio_track_state;
  experiment_features.reset_audio_decoder =
      extension_features->reset_audio_decoder;
  experiment_features.video_decoder_initial_preroll_count =
      FromIntPointer(extension_features->video_decoder_initial_preroll_count);
  experiment_features.video_decoder_poll_interval_ms =
      FromIntPointer(extension_features->video_decoder_poll_interval_ms);
  experiment_features.video_initial_max_frames_in_decoder =
      FromIntPointer(extension_features->video_initial_max_frames_in_decoder);
  experiment_features.video_max_pending_input_frames =
      FromIntPointer(extension_features->video_max_pending_input_frames);
  experiment_features.video_renderer_min_decoded_frames =
      FromIntPointer(extension_features->video_renderer_min_decoded_frames);
  experiment_features.video_renderer_min_input_buffers =
      FromIntPointer(extension_features->video_renderer_min_input_buffers);

  *GetOrCreateExperimentalFeatures() = experiment_features;
}

const ExperimentalFeatures& GetExperimentalFeaturesForCurrentThread() {
  ExperimentalFeatures* current = GetExperimentalFeaturesTLS().Get();
  SB_CHECK(current)
      << "ExperimentalFeatures are not set. This method was likely "
         "called on the wrong thread or a race condition occurred.";
  return *current;
}

const void* GetExperimentalFeaturesConfigurationApi() {
  return &kExperimentalFeaturesConfigurationApi;
}

}  // namespace starboard::shared::starboard
