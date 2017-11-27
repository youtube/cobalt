// Copyright 2016 Google Inc. All Rights Reserved.
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

#ifndef STARBOARD_SHARED_STARBOARD_AUDIO_SINK_STUB_AUDIO_SINK_TYPE_H_
#define STARBOARD_SHARED_STARBOARD_AUDIO_SINK_STUB_AUDIO_SINK_TYPE_H_

#include "starboard/log.h"
#include "starboard/shared/internal_only.h"
#include "starboard/shared/starboard/audio_sink/audio_sink_internal.h"

namespace starboard {
namespace shared {
namespace starboard {
namespace audio_sink {

class StubAudioSinkType : public SbAudioSinkPrivate::Type {
 public:
  SbAudioSink Create(
      int channels,
      int sampling_frequency_hz,
      SbMediaAudioSampleType audio_sample_type,
      SbMediaAudioFrameStorageType audio_frame_storage_type,
      SbAudioSinkFrameBuffers frame_buffers,
      int frame_buffers_size_in_frames,
      SbAudioSinkUpdateSourceStatusFunc update_source_status_func,
      SbAudioSinkConsumeFramesFunc consume_frames_func,
      void* context);

  bool IsValid(SbAudioSink audio_sink) override {
    return audio_sink != kSbAudioSinkInvalid && audio_sink->IsType(this);
  }

  void Destroy(SbAudioSink audio_sink) override {
    if (audio_sink != kSbAudioSinkInvalid && !IsValid(audio_sink)) {
      SB_LOG(WARNING) << "audio_sink is invalid.";
      return;
    }
    delete audio_sink;
  }
};

}  // namespace audio_sink
}  // namespace starboard
}  // namespace shared
}  // namespace starboard

#endif  // STARBOARD_SHARED_STARBOARD_AUDIO_SINK_STUB_AUDIO_SINK_TYPE_H_
