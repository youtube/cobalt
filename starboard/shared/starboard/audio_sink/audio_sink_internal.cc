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

#include "starboard/shared/starboard/audio_sink/audio_sink_internal.h"

#include <functional>

#include "starboard/common/log.h"
#include "starboard/shared/starboard/application.h"
#include "starboard/shared/starboard/audio_sink/stub_audio_sink_type.h"
#include "starboard/shared/starboard/command_line.h"

namespace {

using std::placeholders::_1;
using std::placeholders::_2;
using std::placeholders::_3;

bool is_fallback_to_stub_enabled;
SbAudioSinkPrivate::Type* primary_audio_sink_type;
SbAudioSinkPrivate::Type* fallback_audio_sink_type;

// Command line switch that controls whether we default to the stub audio sink,
// even when the primary audio sink may be available.
const char kUseStubAudioSink[] = "use_stub_audio_sink";

void WrapConsumeFramesFunc(SbAudioSinkConsumeFramesFunc sb_consume_frames_func,
                           int frames_consumed,
                           SbTime frames_consumed_at,
                           void* context) {
#if SB_API_VERSION >= 12
  SB_UNREFERENCED_PARAMETER(frames_consumed_at);
  sb_consume_frames_func(frames_consumed, context);
#else  // SB_API_VERSION >= 12
  sb_consume_frames_func(frames_consumed,
#if SB_HAS(SB_HAS_ASYNC_AUDIO_FRAMES_REPORTING)
                         frames_consumed_at,
#endif
                         context);
#endif  // SB_API_VERSION >=  12
}

}  // namespace

using starboard::shared::starboard::audio_sink::StubAudioSinkType;

// static
void SbAudioSinkPrivate::Initialize() {
  fallback_audio_sink_type = new StubAudioSinkType;
  PlatformInitialize();
}

// static
void SbAudioSinkPrivate::TearDown() {
  PlatformTearDown();
  delete fallback_audio_sink_type;
  fallback_audio_sink_type = NULL;
}

// static
void SbAudioSinkPrivate::SetPrimaryType(Type* type) {
  primary_audio_sink_type = type;
}

// static
SbAudioSinkPrivate::Type* SbAudioSinkPrivate::GetPrimaryType() {
  return primary_audio_sink_type;
}
// static
void SbAudioSinkPrivate::EnableFallbackToStub() {
  is_fallback_to_stub_enabled = true;
}
// static
SbAudioSinkPrivate::Type* SbAudioSinkPrivate::GetFallbackType() {
  if (is_fallback_to_stub_enabled) {
    return fallback_audio_sink_type;
  }
  return NULL;
}

// static
SbAudioSinkPrivate::Type* SbAudioSinkPrivate::GetPreferredType() {
  SbAudioSinkPrivate::Type* audio_sink_type = NULL;
  auto command_line =
      starboard::shared::starboard::Application::Get()->GetCommandLine();
  if (!command_line->HasSwitch(kUseStubAudioSink)) {
    audio_sink_type = SbAudioSinkPrivate::GetPrimaryType();
  }
  if (!audio_sink_type) {
    SB_LOG(WARNING) << "Primary audio sink type not selected or missing, "
                       "opting to use Fallback instead.";
    audio_sink_type = SbAudioSinkPrivate::GetFallbackType();
  }
  if (audio_sink_type == NULL) {
    SB_LOG(WARNING) << "Fallback audio sink type is not enabled.";
  }
  return audio_sink_type;
}

SbAudioSink SbAudioSinkPrivate::Create(
    int channels,
    int sampling_frequency_hz,
    SbMediaAudioSampleType audio_sample_type,
    SbMediaAudioFrameStorageType audio_frame_storage_type,
    SbAudioSinkFrameBuffers frame_buffers,
    int frame_buffers_size_in_frames,
    SbAudioSinkUpdateSourceStatusFunc update_source_status_func,
    ConsumeFramesFunc consume_frames_func,
#if SB_API_VERSION >= 12
    ErrorFunc error_func,
#endif  // SB_API_VERSION >= 12
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

  if (!consume_frames_func) {
    SB_LOG(WARNING) << "consume_frames_func cannot be NULL";
    return kSbAudioSinkInvalid;
  }

  if (auto audio_sink_type = GetPreferredType()) {
    auto audio_sink = audio_sink_type->Create(
        channels, sampling_frequency_hz, audio_sample_type,
        audio_frame_storage_type, frame_buffers, frame_buffers_size_in_frames,
        update_source_status_func, consume_frames_func,
#if SB_API_VERSION >= 12
        error_func,
#endif  // SB_API_VERSION >= 12
        context);
    if (audio_sink_type->IsValid(audio_sink)) {
      return audio_sink;
    }
    SB_LOG(ERROR) << "Failed to create SbAudioSink from preferred type.";
    audio_sink_type->Destroy(audio_sink);
  } else {
    SB_LOG(WARNING) << "Preferred Sink Type is invalid.";
  }

  SB_LOG(WARNING) << "Try to create AudioSink using fallback type.";
  if (auto fallback_type = SbAudioSinkPrivate::GetFallbackType()) {
    auto audio_sink = fallback_type->Create(
        channels, sampling_frequency_hz, audio_sample_type,
        audio_frame_storage_type, frame_buffers, frame_buffers_size_in_frames,
        update_source_status_func, consume_frames_func,
#if SB_API_VERSION >= 12
        error_func,
#endif  // SB_API_VERSION >= 12
        context);
    if (fallback_type->IsValid(audio_sink)) {
      return audio_sink;
    }
    SB_LOG(ERROR) << "Failed to create SbAudioSink from Fallback type.";
    fallback_type->Destroy(audio_sink);
  } else {
    SB_LOG(WARNING) << "Fallback Sink Type is invalid.";
  }
  return kSbAudioSinkInvalid;
}

// static
SbAudioSink SbAudioSinkPrivate::Create(
    int channels,
    int sampling_frequency_hz,
    SbMediaAudioSampleType audio_sample_type,
    SbMediaAudioFrameStorageType audio_frame_storage_type,
    SbAudioSinkFrameBuffers frame_buffers,
    int frame_buffers_size_in_frames,
    SbAudioSinkUpdateSourceStatusFunc update_source_status_func,
    SbAudioSinkConsumeFramesFunc sb_consume_frames_func,
#if SB_API_VERSION >= 12
    ErrorFunc error_func,
#endif  // SB_API_VERSION >= 12
    void* context) {
  return Create(channels, sampling_frequency_hz, audio_sample_type,
                audio_frame_storage_type, frame_buffers,
                frame_buffers_size_in_frames, update_source_status_func,
                sb_consume_frames_func
                    ? std::bind(&::WrapConsumeFramesFunc,
                                sb_consume_frames_func, _1, _2, _3)
                    : ConsumeFramesFunc(),
#if SB_API_VERSION >= 12
                error_func,
#endif  // SB_API_VERSION >= 12
                context);
}
