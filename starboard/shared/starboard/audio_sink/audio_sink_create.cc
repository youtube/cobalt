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

#include "starboard/audio_sink.h"

#include "starboard/common/log.h"
#include "starboard/shared/starboard/audio_sink/audio_sink_internal.h"

SbAudioSink SbAudioSinkCreate(
    int channels,
    int sampling_frequency_hz,
    SbMediaAudioSampleType audio_sample_type,
    SbMediaAudioFrameStorageType audio_frame_storage_type,
    SbAudioSinkFrameBuffers frame_buffers,
    int frame_buffers_size_in_frames,
    SbAudioSinkUpdateSourceStatusFunc update_source_status_func,
    SbAudioSinkConsumeFramesFunc consume_frames_func,
    void* context) {
  return SbAudioSinkPrivate::Create(
      channels, sampling_frequency_hz, audio_sample_type,
      audio_frame_storage_type, frame_buffers, frame_buffers_size_in_frames,
      update_source_status_func, consume_frames_func,
#if SB_API_VERSION >= 12
      NULL /*error_func*/,
#endif  // SB_API_VERSION >= 12
      context);
}
