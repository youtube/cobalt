// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_AUDIO_ANDROID_OPENSLES_OUTPUT_H_
#define MEDIA_AUDIO_ANDROID_OPENSLES_OUTPUT_H_

#include <vector>

#include "base/compiler_specific.h"
#include "media/audio/android/opensles_util.h"
#include "media/audio/audio_io.h"
#include "media/audio/audio_parameters.h"
#include <SLES/OpenSLES_Android.h>

namespace media {

class AudioManagerAndroid;

// Implements PCM audio output support for Android using the OpenSLES API.
class OpenSLESOutputStream : public AudioOutputStream {
 public:
  static const int kNumOfQueuesInBuffer = 2;

  OpenSLESOutputStream(AudioManagerAndroid* manager,
                       const AudioParameters& params);

  virtual ~OpenSLESOutputStream();

  // Implementation of AudioOutputStream.
  virtual bool Open() OVERRIDE;
  virtual void Close() OVERRIDE;
  virtual void Start(AudioSourceCallback* callback) OVERRIDE;
  virtual void Stop() OVERRIDE;
  virtual void SetVolume(double volume) OVERRIDE;
  virtual void GetVolume(double* volume) OVERRIDE;

 private:
  bool CreatePlayer();

  static void SimpleBufferQueueCallback(
      SLAndroidSimpleBufferQueueItf buffer_queue, void* instance);

  void FillBufferQueue();

  // Called in Open();
  void SetupAudioBuffer();

  // Called in Close();
  void ReleaseAudioBuffer();

  // If OpenSLES reports an error this function handles it and passes it to
  // the attached AudioOutputCallback::OnError().
  void HandleError(SLresult error);

  AudioManagerAndroid* audio_manager_;

  AudioSourceCallback* callback_;

  // Shared engine interfaces for the app.
  media::ScopedSLObjectItf engine_object_;
  media::ScopedSLObjectItf player_object_;
  media::ScopedSLObjectItf output_mixer_;

  SLPlayItf player_;

  // Buffer queue recorder interface.
  SLAndroidSimpleBufferQueueItf simple_buffer_queue_;

  SLDataFormat_PCM format_;

  // Audio buffer arrays that are allocated in the constructor.
  uint8* audio_data_[kNumOfQueuesInBuffer];

  int active_queue_;
  size_t buffer_size_bytes_;

  bool started_;

  // Volume level from 0 to 1.
  float volume_;

  // Container for retrieving data from AudioSourceCallback::OnMoreData().
  scoped_ptr<AudioBus> audio_bus_;

  DISALLOW_COPY_AND_ASSIGN(OpenSLESOutputStream);
};

}  // namespace media

#endif  // MEDIA_AUDIO_ANDROID_OPENSLES_INPUT_H_
