//
// Copyright 2020 Comcast Cable Communications Management, LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
// SPDX-License-Identifier: Apache-2.0
//
// Copyright 2016 The Cobalt Authors. All Rights Reserved.
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

#ifndef THIRD_PARTY_STARBOARD_RDK_SHARED_AUDIO_SINK_GSTREAMER_AUDIO_SINK_TYPE_H_
#define THIRD_PARTY_STARBOARD_RDK_SHARED_AUDIO_SINK_GSTREAMER_AUDIO_SINK_TYPE_H_

#include "starboard/common/log.h"
#include "starboard/shared/starboard/audio_sink/audio_sink_internal.h"
#include "third_party/starboard/rdk/shared/log_override.h"

namespace third_party {
namespace starboard {
namespace rdk {
namespace shared {
namespace audio_sink {

class GStreamerAudioSinkType : public SbAudioSinkPrivate::Type {
 public:
  SbAudioSink Create(
      int channels,
      int sampling_frequency_hz,
      SbMediaAudioSampleType audio_sample_type,
      SbMediaAudioFrameStorageType audio_frame_storage_type,
      SbAudioSinkFrameBuffers frame_buffers,
      int frame_buffers_size_in_frames,
      SbAudioSinkUpdateSourceStatusFunc update_source_status_func,
      SbAudioSinkPrivate::ConsumeFramesFunc consume_frames_func,
      SbAudioSinkPrivate::ErrorFunc error_func,
      void* context) override;

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

  static GStreamerAudioSinkType* CreateInstance();
  static void DestroyInstance(GStreamerAudioSinkType* instance);

 private:
  GStreamerAudioSinkType() = default;
  ~GStreamerAudioSinkType() = default;
};

}  // namespace audio_sink
}  // namespace shared
}  // namespace rdk
}  // namespace starboard
}  // namespace third_party

#endif  // THIRD_PARTY_STARBOARD_RDK_SHARED_AUDIO_SINK_GSTREAMER_AUDIO_SINK_TYPE_H_
