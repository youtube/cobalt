// Copyright (c) 2016 Samsung Electronics Co., Ltd All Rights Reserved
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
#include "starboard/log.h"
#include "starboard/mutex.h"
#include "starboard/contrib/tizen/shared/audio/audio_sink_private.h"

#define CHECK_CAPI_AUDIO_ERROR(func)                            \
  if (capi_ret != AUDIO_IO_ERROR_NONE) {                        \
    SB_DLOG(ERROR) << "[MEDIA] " #func " (" << capi_ret << ", " \
                   << GetCAPIErrorString(capi_ret) << ")";      \
    return;                                                     \
  }

const int kSampleByte = 2;

SbAudioSinkPrivate::SbAudioSinkPrivate(
    int channels,
    int sampling_frequency_hz,
    SbMediaAudioSampleType audio_sample_type,
    SbMediaAudioFrameStorageType audio_frame_storage_type,
    SbAudioSinkFrameBuffers frame_buffers,
    int frames_per_channel,
    SbAudioSinkUpdateSourceStatusFunc update_source_status_func,
    SbAudioSinkConsumeFramesFunc consume_frames_func,
    void* context)
    : channels_(channels),
      sampling_frequency_hz_(sampling_frequency_hz),
      audio_sample_type_(audio_sample_type),
      audio_frame_storage_type_(audio_frame_storage_type),
      frame_buffers_(frame_buffers),
      frames_per_channel_(frames_per_channel),
      update_source_status_func_(update_source_status_func),
      consume_frames_func_(consume_frames_func),
      context_(context),
      destroying_(false),
      is_paused_(true) {
  SB_DLOG(INFO) << "[MEDIA] SbAudioSinkPrivate : "
                << "channels " << channels << ", frequency "
                << sampling_frequency_hz << ", sample_type "
                << audio_sample_type << ", storage_type "
                << audio_frame_storage_type << ", frame_buffers " << std::hex
                << frame_buffers << ", frame_buff_sz " << frames_per_channel;

  int capi_ret;
  capi_ret = audio_out_create_new(sampling_frequency_hz, AUDIO_CHANNEL_STEREO,
                                  AUDIO_SAMPLE_TYPE_S16_LE,  // kSampleByte = 2
                                  &capi_audio_out_);
  CHECK_CAPI_AUDIO_ERROR(audio_out_create_new);

  capi_ret = audio_out_set_interrupted_cb(capi_audio_out_,
                                          OnCAPIAudioIOInterrupted_CB, this);
  CHECK_CAPI_AUDIO_ERROR(audio_out_set_interrupted_cb);

  // Starts the thread
  audio_out_thread_ =
      SbThreadCreate(0, kSbThreadPriorityRealTime, kSbThreadNoAffinity, true,
                     "tizen_audio_out", AudioSinkThreadProc_CB, this);
  SB_DCHECK(SbThreadIsValid(audio_out_thread_));
}

SbAudioSinkPrivate::~SbAudioSinkPrivate() {
  // Stop the thread
  {
    starboard::ScopedLock lock(mutex_);
    destroying_ = true;
  }
  if (SbThreadIsValid(audio_out_thread_)) {
    SB_DLOG(INFO) << "[MEDIA] wait for audio sink thread exit";
    SbThreadJoin(audio_out_thread_, NULL);
  } else {
    SB_DLOG(INFO) << "[MEDIA] audio sink thread is invalid. skip waiting";
  }

  // destroy capi audio
  if (capi_audio_out_) {
    int ret = audio_out_destroy(capi_audio_out_);
    if (ret != AUDIO_IO_ERROR_NONE) {
      SB_DLOG(ERROR) << "audio_out_destroy failed (" << ret << ")";
    }
  }
}

bool SbAudioSinkPrivate::IsValid() {
  return SbThreadIsValid(audio_out_thread_);
}

// static callbacks
void SbAudioSinkPrivate::OnCAPIAudioIOInterrupted_CB(
    audio_io_interrupted_code_e code,
    void* user_data) {
  SbAudioSinkPrivate* audio_sink =
      reinterpret_cast<SbAudioSinkPrivate*>(user_data);
  if (audio_sink) {
    audio_sink->OnCAPIAudioIOInterrupted(code);
  }
}
void SbAudioSinkPrivate::OnCAPIAudioStreamWrite_CB(
    audio_out_h handle,
    size_t nbytes,
    void* user_data) {  // not used
  SbAudioSinkPrivate* audio_sink =
      reinterpret_cast<SbAudioSinkPrivate*>(user_data);
  if (audio_sink) {
    audio_sink->OnCAPIAudioStreamWrite(handle, nbytes);
  }
}
void* SbAudioSinkPrivate::AudioSinkThreadProc_CB(void* context) {
  SbAudioSinkPrivate* audio_sink =
      reinterpret_cast<SbAudioSinkPrivate*>(context);
  if (audio_sink) {
    return audio_sink->AudioSinkThreadProc();
  }
  return NULL;
}

const char* SbAudioSinkPrivate::GetCAPIErrorString(int ret) {
  // TODO : Get CAPI error and print log
  return "Unknown";
}

void SbAudioSinkPrivate::OnCAPIAudioIOInterrupted(
    audio_io_interrupted_code_e code) {
  SB_DLOG(WARNING) << "Play interrupted: audio_io_interrupted_code_e : "
                   << code;
}

void SbAudioSinkPrivate::OnCAPIAudioStreamWrite(audio_out_h handle,
                                                size_t nbytes) {
  SB_DLOG(INFO) << "[MEDIA] OnAudioStreamWrite (not used) - request " << nbytes;
}

void* SbAudioSinkPrivate::AudioSinkThreadProc() {
  void* buf;
  int bytes_to_fill;
  int bytes_written;
  int bytes_per_frame = kSampleByte;
  int consumed_frames;

  SB_DLOG(INFO) << "[MEDIA] sink thread started";

  for (;;) {
    {
      starboard::ScopedLock lock(mutex_);
      if (destroying_) {
        break;
      }
    }

    int frames_in_buffer, offset_in_frames;
    bool is_playing, is_eos_reached;
    update_source_status_func_(&frames_in_buffer, &offset_in_frames,
                               &is_playing, &is_eos_reached, context_);

    if (is_playing) {
      buf = reinterpret_cast<uint8_t*>(frame_buffers_[0]) +
            offset_in_frames * bytes_per_frame;
      if (offset_in_frames + frames_in_buffer <= frames_per_channel_) {
        bytes_to_fill = frames_in_buffer * bytes_per_frame;
      } else {
        bytes_to_fill =
            (frames_per_channel_ - offset_in_frames) * bytes_per_frame;
      }

      if (is_paused_) {
        // audio_out_resume(capi_audio_out_);
        audio_out_prepare(capi_audio_out_);
        is_paused_ = false;
        SB_DLOG(INFO) << "[MEDIA] audio_out_resume";
      }

      bytes_written = audio_out_write(capi_audio_out_, buf, bytes_to_fill);

      if (bytes_written < 0) {
        SB_DLOG(ERROR) << "[MEDIA] audio_out_write error (" << bytes_written
                       << ", " << GetCAPIErrorString(bytes_written) << ")";
        break;
      }
      consumed_frames = bytes_written / bytes_per_frame;

      // This is commented : Sleep can cause 'underrun'
      // update_source_status_func controls data's timing.
      // SbThreadSleep(consumed_frames * kSbTimeSecond /
      // sampling_frequency_hz_);
      consume_frames_func_(consumed_frames, context_);
    } else {
      if (!is_paused_) {
        audio_out_drain(capi_audio_out_);
        audio_out_unprepare(capi_audio_out_);
        is_paused_ = true;
        SB_DLOG(INFO) << "[MEDIA] audio_out_pause";
      }
      // Wait for five millisecond if we are paused.
      SbThreadSleep(kSbTimeMillisecond * 5);
    }
  }

  SB_DLOG(INFO) << "[MEDIA] sink thread exited";
  return NULL;
}
