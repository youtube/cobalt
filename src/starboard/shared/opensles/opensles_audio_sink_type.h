// Copyright (c) 2016, NVIDIA CORPORATION.  All rights reserved.
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

#ifndef STARBOARD_SHARED_OPENSELS_AUDIO_SINK_TYPE_H_
#define STARBOARD_SHARED_OPENSELS_AUDIO_SINK_TYPE_H_

#include <SLES/OpenSLES.h>
#include <SLES/OpenSLES_Android.h>

#include "starboard/audio_sink.h"
#include "starboard/configuration.h"
#include "starboard/log.h"
#include "starboard/shared/internal_only.h"
#include "starboard/shared/starboard/audio_sink/audio_sink_internal.h"

namespace starboard {
namespace shared {
namespace opensles {

class OpenSLESAudioSinkType : public SbAudioSinkPrivate::Type {
 public:
  SLObjectItf engineObj_;
  SLEngineItf engineItf_;
  SLObjectItf outputMix_;

 public:
  OpenSLESAudioSinkType(); SB_OVERRIDE
  ~OpenSLESAudioSinkType(); SB_OVERRIDE

  SbAudioSink Create(
      int channels,
      int sampling_frequency_hz,
      SbMediaAudioSampleType audio_sample_type,
      SbMediaAudioFrameStorageType audio_frame_storage_type,
      SbAudioSinkFrameBuffers frame_buffers,
      int frames_per_channel,
      SbAudioSinkUpdateSourceStatusFunc update_source_status_func,
      SbAudioSinkConsumeFramesFunc consume_frames_func,
      void* context);

  bool IsValid(SbAudioSink audio_sink) SB_OVERRIDE {
    return audio_sink != kSbAudioSinkInvalid && audio_sink->IsType(this);
  }

  void Destroy(SbAudioSink audio_sink) SB_OVERRIDE {
    if (audio_sink != kSbAudioSinkInvalid && !IsValid(audio_sink)) {
      SB_LOG(WARNING) << "audio_sink is invalid.";
      return;
    }
    delete audio_sink;
  }
};

}  // namespace opensles
}  // namespace shared
}  // namespace starboard

#endif  // STARBOARD_SHARED_OPENSLES_AUDIO_SINK_TYPE_H_
