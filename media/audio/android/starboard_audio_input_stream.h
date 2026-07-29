// Copyright 2026 The Cobalt Authors. All Rights Reserved.
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
//
// Starboard specific low latency path based on
// media/audio/android/opensles_input.cc

#ifndef MEDIA_AUDIO_ANDROID_STARBOARD_AUDIO_INPUT_STREAM_H_
#define MEDIA_AUDIO_ANDROID_STARBOARD_AUDIO_INPUT_STREAM_H_

#include <SLES/OpenSLES.h>
#include <SLES/OpenSLES_Android.h>
#include <stdint.h>

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/synchronization/lock.h"
#include "base/threading/thread_checker.h"
#include "base/time/time.h"
#include "media/audio/android/opensles_util.h"
#include "media/audio/audio_io.h"
#include "media/base/audio_parameters.h"
#include "cobalt/media/audio/audio_input_constants.h"

namespace media {

class AudioBus;
class AudioManagerAndroid;

// StarboardAudioInputStream is a simplified version of OpenSLESInputStream
// specifically tuned for low-latency voice recognition on Cobalt/Android.
// It bypasses complex negotiation and is hardcoded for 16kHz Mono.
class StarboardAudioInputStream : public AudioInputStream {
 public:
  static const int kMaxNumOfBuffersInQueue = 2;
  static const int kDefaultBufferSizeInBytes = 1024;

  StarboardAudioInputStream(AudioManagerAndroid* manager,
                            const AudioParameters& params);

  StarboardAudioInputStream(const StarboardAudioInputStream&) = delete;
  StarboardAudioInputStream& operator=(const StarboardAudioInputStream&) = delete;

  ~StarboardAudioInputStream() override;

  // Requests the OS-level runtime permission for audio recording.
  // Returns true if permission is granted, false if denied or pending.
  static bool RequestRuntimePermission();

  // Implementation of AudioInputStream.
  OpenOutcome Open() override;
  void Start(AudioInputCallback* callback) override;
  void Stop() override;
  void Close() override;
  double GetMaxVolume() override;
  void SetVolume(double volume) override;
  double GetVolume() override;
  bool SetAutomaticGainControl(bool enabled) override;
  bool GetAutomaticGainControl() override;
  bool IsMuted() override;
  void SetOutputDeviceForAec(const std::string& output_device_id) override;

 private:
  bool CreateRecorder();

  // Called from OpenSLES specific audio worker thread.
  static void SimpleBufferQueueCallback(
      SLAndroidSimpleBufferQueueItf buffer_queue,
      void* instance);

  // Called from OpenSLES specific audio worker thread.
  void ReadBufferQueue();

  // Called in Open();
  void SetupAudioBuffer();

  // Called in Close();
  void ReleaseAudioBuffer();

  // If OpenSLES reports an error this function handles it and passes it to
  // the attached AudioInputCallback::OnError().
  void HandleError(SLresult error);

  base::ThreadChecker thread_checker_;

  base::Lock lock_;

  const raw_ptr<AudioManagerAndroid> audio_manager_ = nullptr;

  raw_ptr<AudioInputCallback> callback_ GUARDED_BY(lock_) = nullptr;

  // Shared engine interfaces for the app.
  media::ScopedSLObjectItf recorder_object_;
  media::ScopedSLObjectItf engine_object_;

  SLRecordItf recorder_ = nullptr;

  // Buffer queue recorder interface.
  SLAndroidSimpleBufferQueueItf simple_buffer_queue_ GUARDED_BY(lock_) = nullptr;

  const SLAndroidDataFormat_PCM_EX format_;

  // Audio buffers that are allocated in SetupAudioBuffer().
  std::unique_ptr<uint8_t[]> audio_data_[kMaxNumOfBuffersInQueue] GUARDED_BY(lock_);

  int active_buffer_index_ GUARDED_BY(lock_) = 0;
  const int buffer_size_bytes_ GUARDED_BY(lock_) = 0;

  bool started_ = false;

  const std::unique_ptr<media::AudioBus> audio_bus_;

  const base::TimeDelta hardware_delay_;

  base::WeakPtrFactory<StarboardAudioInputStream> weak_factory_{this};
};

}  // namespace media

#endif  // MEDIA_AUDIO_ANDROID_STARBOARD_AUDIO_INPUT_STREAM_H_
