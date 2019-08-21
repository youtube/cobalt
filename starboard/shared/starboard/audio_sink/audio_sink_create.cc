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
#include "starboard/shared/starboard/audio_sink/audio_sink_type.h"

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
  if (channels <= 0 || channels > SbAudioSinkGetMaxChannels()) {
    SB_LOG(WARNING) << "Invalid audio channels " << channels;
    return kSbAudioSinkInvalid;
  }

  if (sampling_frequency_hz <= 0) {
    SB_LOG(WARNING) << "Invalid audio sampling frequency "
                    << sampling_frequency_hz;
    return kSbAudioSinkInvalid;
  }

  if (!SbAudioSinkIsAudioSampleTypeSupported(audio_sample_type)) {
    SB_LOG(WARNING) << "Invalid audio sample type " << audio_sample_type;
    return kSbAudioSinkInvalid;
  }

  if (!SbAudioSinkIsAudioFrameStorageTypeSupported(audio_frame_storage_type)) {
    SB_LOG(WARNING) << "Invalid audio frame storage type "
                    << audio_frame_storage_type;
    return kSbAudioSinkInvalid;
  }

  if (frame_buffers == NULL) {
    SB_LOG(WARNING) << "Pointer to frame buffers cannot be NULL";
    return kSbAudioSinkInvalid;
  }

  if (frame_buffers_size_in_frames <= 0) {
    SB_LOG(WARNING) << "Invalid frame buffer size "
                    << frame_buffers_size_in_frames;
    return kSbAudioSinkInvalid;
  }

  if (update_source_status_func == NULL) {
    SB_LOG(WARNING) << "update_source_status_func cannot be NULL";
    return kSbAudioSinkInvalid;
  }

  if (consume_frames_func == NULL) {
    SB_LOG(WARNING) << "consume_frames_func cannot be NULL";
    return kSbAudioSinkInvalid;
  }

  auto audio_sink_type = SbAudioSinkPrivate::GetPreferredAudioSinkType();
  if (!audio_sink_type) {
    return kSbAudioSinkInvalid;
  }
  SbAudioSink audio_sink = audio_sink_type->Create(
      channels, sampling_frequency_hz, audio_sample_type,
      audio_frame_storage_type, frame_buffers, frame_buffers_size_in_frames,
      update_source_status_func, consume_frames_func, context);
  if (audio_sink_type->IsValid(audio_sink)) {
    return audio_sink;
  }
  audio_sink_type->Destroy(audio_sink);
  auto fallback_audio_sink_type =
      SbAudioSinkPrivate::GetFallbackAudioSinkType();
  if (!fallback_audio_sink_type) {
    return kSbAudioSinkInvalid;
  }
  audio_sink = fallback_audio_sink_type->Create(
      channels, sampling_frequency_hz, audio_sample_type,
      audio_frame_storage_type, frame_buffers, frame_buffers_size_in_frames,
      update_source_status_func, consume_frames_func, context);
  if (fallback_audio_sink_type->IsValid(audio_sink)) {
    return audio_sink;
  }
  fallback_audio_sink_type->Destroy(audio_sink);
  return kSbAudioSinkInvalid;
}
