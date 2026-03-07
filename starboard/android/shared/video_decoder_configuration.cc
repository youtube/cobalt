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

#include "starboard/android/shared/video_decoder_configuration.h"

#include "starboard/android/shared/video_decoder_configuration_internal.h"
#include "starboard/common/log.h"
#include "starboard/extension/video_decoder_configuration.h"

namespace starboard::android::shared {

namespace {

// Definitions of any functions included as components in the extension
// are added here.

void SetVideoConfiguration(
    const StarboardExtensionVideoConfiguration* configuration) {
  // 'configuration' is always non-null; passed as a pointer for C
  // compatibility.
  SB_CHECK(configuration);

  VideoConfiguration internal_configuration{};
  if (configuration->renderer_min_input_buffers != nullptr) {
    internal_configuration.renderer_min_input_buffers =
        *configuration->renderer_min_input_buffers;
  }
  if (configuration->renderer_min_decoded_frames != nullptr) {
    internal_configuration.renderer_min_decoded_frames =
        *configuration->renderer_min_decoded_frames;
  }

  SetVideoConfigurationForCurrentThread(internal_configuration);
}

const StarboardExtensionVideoDecoderConfigurationApi
    kVideoDecoderConfigurationApi = {
        kStarboardExtensionVideoDecoderConfigurationName,
        2,
        &SetVideoInitialMaxFramesInDecoderForCurrentThread,
        &SetVideoMaxPendingInputFramesForCurrentThread,
        &SetVideoDecoderInitialPrerollCountForCurrentThread,
        &SetVideoDecoderPollIntervalMsForCurrentThread,
        &SetMediaCodecResetDelayMsForCurrentThread,
        &SetVideoConfiguration,
};

}  // namespace

const void* GetVideoDecoderConfigurationApi() {
  return &kVideoDecoderConfigurationApi;
}

}  // namespace starboard::android::shared
