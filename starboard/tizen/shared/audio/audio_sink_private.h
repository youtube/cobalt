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

#ifndef STARBOARD_TIZEN_SHARED_AUDIO_AUDIO_SINK_PRIVATE_H_
#define STARBOARD_TIZEN_SHARED_AUDIO_AUDIO_SINK_PRIVATE_H_

#include <audio_io.h>

#include "starboard/audio_sink.h"
#include "starboard/mutex.h"
#include "starboard/thread.h"
#include "starboard/time.h"

// starboard object
struct SbAudioSinkPrivate {
 public:
  SbAudioSinkPrivate(
      int channels,
      int sampling_frequency_hz,
      SbMediaAudioSampleType audio_sample_type,
      SbMediaAudioFrameStorageType audio_frame_storage_type,
      SbAudioSinkFrameBuffers frame_buffers,
      int frames_per_channel,
      SbAudioSinkUpdateSourceStatusFunc update_source_status_func,
      SbAudioSinkConsumeFramesFunc consume_frames_func,
      void* context);
  virtual ~SbAudioSinkPrivate();

  bool IsValid();

  // callbacks
  static void OnCAPIAudioIOInterrupted_CB(audio_io_interrupted_code_e code,
                                          void* user_data);
  static void OnCAPIAudioStreamWrite_CB(audio_out_h handle,
                                        size_t nbytes,
                                        void* user_data);
  static void* AudioSinkThreadProc_CB(void* context);

 private:
  const char* GetCAPIErrorString(int capi_ret);

  void OnCAPIAudioIOInterrupted(audio_io_interrupted_code_e code);
  void OnCAPIAudioStreamWrite(audio_out_h handle, size_t nbytes);
  void* AudioSinkThreadProc();

  // setting of creation
  int channels_;
  int sampling_frequency_hz_;
  SbMediaAudioSampleType audio_sample_type_;
  SbMediaAudioFrameStorageType audio_frame_storage_type_;
  SbAudioSinkFrameBuffers frame_buffers_;
  int frames_per_channel_;
  SbAudioSinkUpdateSourceStatusFunc update_source_status_func_;
  SbAudioSinkConsumeFramesFunc consume_frames_func_;
  void* context_;

  // thread related
  bool destroying_;
  SbThread audio_out_thread_;
  ::starboard::Mutex mutex_;

  // capi related
  audio_out_h capi_audio_out_;
  bool is_paused_;
};

#endif  // STARBOARD_TIZEN_SHARED_AUDIO_AUDIO_SINK_PRIVATE_H_
