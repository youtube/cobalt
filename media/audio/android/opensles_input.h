// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_AUDIO_ANDROID_OPENSLES_INPUT_H_
#define MEDIA_AUDIO_ANDROID_OPENSLES_INPUT_H_

#include "base/compiler_specific.h"
#include "media/audio/audio_io.h"
#include "media/audio/audio_parameters.h"
#include "media/audio/android/opensles_util.h"
#include <SLES/OpenSLES_Android.h>

namespace media {

class AudioManagerAndroid;

// Implements PCM audio input support for Android using the OpenSLES API.
class OpenSLESInputStream : public AudioInputStream {
 public:
  static const int kNumOfQueuesInBuffer = 2;

  OpenSLESInputStream(AudioManagerAndroid* manager,
                      const AudioParameters& params);

  virtual ~OpenSLESInputStream();

  // Implementation of AudioInputStream.
  virtual bool Open() OVERRIDE;
  virtual void Start(AudioInputCallback* callback) OVERRIDE;
  virtual void Stop() OVERRIDE;
  virtual void Close() OVERRIDE;
  virtual double GetMaxVolume() OVERRIDE;
  virtual void SetVolume(double volume) OVERRIDE;
  virtual double GetVolume() OVERRIDE;
  virtual void SetAutomaticGainControl(bool enabled) OVERRIDE;
  virtual bool GetAutomaticGainControl() OVERRIDE;

 private:
  bool CreateRecorder();

  static void SimpleBufferQueueCallback(
      SLAndroidSimpleBufferQueueItf buffer_queue, void* instance);

  void ReadBufferQueue();

  // Called in Open();
  void SetupAudioBuffer();

  // Called in Close();
  void ReleaseAudioBuffer();

  // If OpenSLES reports an error this function handles it and passes it to
  // the attached AudioInputCallback::OnError().
  void HandleError(SLresult error);

  AudioManagerAndroid* audio_manager_;

  AudioInputCallback* callback_;

  // Shared engine interfaces for the app.
  media::ScopedSLObjectItf recorder_object_;
  media::ScopedSLObjectItf engine_object_;

  SLRecordItf recorder_;

  // Buffer queue recorder interface.
  SLAndroidSimpleBufferQueueItf simple_buffer_queue_;

  SLDataFormat_PCM format_;

  // Audio buffers that are allocated in the constructor based on
  // info from audio parameters.
  uint8* audio_data_[kNumOfQueuesInBuffer];

  int active_queue_;
  int buffer_size_bytes_;

  bool started_;

  DISALLOW_COPY_AND_ASSIGN(OpenSLESInputStream);
};

}  // namespace media

#endif  // MEDIA_AUDIO_ANDROID_OPENSLES_INPUT_H_
