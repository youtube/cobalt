// Copyright 2025 The Cobalt Authors. All Rights Reserved.
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

#ifndef COBALT_MEDIA_AUDIO_AUDIO_HELPERS_H_
#define COBALT_MEDIA_AUDIO_AUDIO_HELPERS_H_

#include "media/base/audio_parameters.h"
#include "starboard/audio_sink.h"
#include "starboard/media.h"

namespace media {

const int kStandardOutputSampleRate = 48000;
const int kPreferredRenderBufferSizeFrames = 1024;

inline media::OutputDeviceInfo GetPreferredOutputParameters() {
  return media::OutputDeviceInfo(
      std::string(), media::OUTPUT_DEVICE_STATUS_OK,
      media::AudioParameters(media::AudioParameters::AUDIO_PCM_LOW_LATENCY,
                             media::ChannelLayoutConfig::Stereo(),
                             kStandardOutputSampleRate,
                             kPreferredRenderBufferSizeFrames));
}

inline SbMediaAudioSampleType GetPreferredOutputStarboardSampleType() {
  if (SbAudioSinkIsAudioSampleTypeSupported(kSbMediaAudioSampleTypeFloat32)) {
    return kSbMediaAudioSampleTypeFloat32;
  }
  return kSbMediaAudioSampleTypeInt16Deprecated;
}

inline size_t GetStarboardSampleTypeSize(SbMediaAudioSampleType sample_type) {
  switch (sample_type) {
    case kSbMediaAudioSampleTypeFloat32:
      return sizeof(float);
    case kSbMediaAudioSampleTypeInt16Deprecated:
      return sizeof(int16_t);
  }
  NOTREACHED();
  return 0u;
}

}  // namespace media

#endif  // COBALT_MEDIA_AUDIO_AUDIO_HELPERS_H_
