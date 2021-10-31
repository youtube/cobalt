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

#ifndef STARBOARD_SHARED_STARBOARD_AUDIO_SINK_AUDIO_SINK_INTERNAL_H_
#define STARBOARD_SHARED_STARBOARD_AUDIO_SINK_AUDIO_SINK_INTERNAL_H_

#include "starboard/audio_sink.h"

#include <functional>
#include <string>

#include "starboard/configuration.h"
#include "starboard/shared/internal_only.h"

struct SbAudioSinkPrivate {
#if SB_API_VERSION >= 12
  // When |capability_changed| is true, it hints that the error is caused by a
  // a transient capability on the platform.  The app should retry playback to
  // recover from the error.
  typedef void (*ErrorFunc)(bool capability_changed,
                            const std::string& error_message,
                            void* context);
#endif  // SB_API_VERSION >= 12

  typedef std::function<
      void(int frames_consumed, SbTime frames_consumed_at, void* context)>
      ConsumeFramesFunc;

  class Type {
   public:
    virtual ~Type() {}

    // |error_func| can be NULL to indicate that there is no error handling.
    virtual SbAudioSink Create(
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
        void* context) = 0;
    virtual bool IsValid(SbAudioSink audio_sink) = 0;
    virtual void Destroy(SbAudioSink audio_sink) = 0;
  };

  virtual ~SbAudioSinkPrivate() {}
  virtual void SetPlaybackRate(double playback_rate) = 0;

  virtual void SetVolume(double volume) = 0;

  virtual bool IsType(Type* type) = 0;

  // The following two functions will be called during application startup and
  // termination.
  static void Initialize();
  static void TearDown();
  // This function has to be called inside PlatformInitialize() and
  // PlatformTearDown() to set or clear the primary audio sink type.
  static void SetPrimaryType(Type* type);
  static Type* GetPrimaryType();
  // This function can only be called inside PlatformInitialize().  Once it is
  // called, GetFallbackType() will return a valid Type that can be used to
  // create audio sink stubs.
  static void EnableFallbackToStub();
  // Return a valid Type to create a fallback audio sink if fallback to stub is
  // enabled.  Otherwise return NULL.
  static Type* GetFallbackType();

  // Return a valid Type, choosing the Primary type if available, otherwise
  // Fallback. If Fallback is not enabled, then returns NULL.
  static Type* GetPreferredType();

  static SbAudioSink Create(
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
      void* context);

  static SbAudioSink Create(
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
      void* context);

  // Individual implementation has to provide implementation of the following
  // functions, which will be called inside Initialize() and TearDown().
  static void PlatformInitialize();
  static void PlatformTearDown();
};

#endif  // STARBOARD_SHARED_STARBOARD_AUDIO_SINK_AUDIO_SINK_INTERNAL_H_
