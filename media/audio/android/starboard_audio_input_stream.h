// Copyright 2024 The Cobalt Authors. All Rights Reserved.

#ifndef MEDIA_AUDIO_ANDROID_STARBOARD_AUDIO_INPUT_STREAM_H_
#define MEDIA_AUDIO_ANDROID_STARBOARD_AUDIO_INPUT_STREAM_H_

#include <SLES/OpenSLES.h>
#include <SLES/OpenSLES_Android.h>
#include <stdint.h>

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/synchronization/lock.h"
#include "base/threading/thread_checker.h"
#include "base/time/time.h"
#include "media/audio/android/opensles_util.h"
#include "media/audio/audio_io.h"
#include "media/base/audio_parameters.h"

namespace media {

class AudioBus;
class AudioManagerAndroid;

// StarboardAudioInputStream is a simplified version of OpenSLESInputStream
// specifically tuned for low-latency voice recognition on Cobalt/Android.
// It bypasses complex negotiation and is hardcoded for 16kHz Mono.
class StarboardAudioInputStream : public AudioInputStream {
 public:
  static const int kMaxNumOfBuffersInQueue = 2;
  static const int kSampleRateHz = 16000;

  StarboardAudioInputStream(AudioManagerAndroid* manager,
                            const AudioParameters& params);

  StarboardAudioInputStream(const StarboardAudioInputStream&) = delete;
  StarboardAudioInputStream& operator=(const StarboardAudioInputStream&) = delete;

  ~StarboardAudioInputStream() override;

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

  // Protects |callback_|, |active_buffer_index_|, |audio_data_|,
  // |buffer_size_bytes_| and |simple_buffer_queue_|.
  base::Lock lock_;

  raw_ptr<AudioManagerAndroid> audio_manager_;

  raw_ptr<AudioInputCallback> callback_;

  // Shared engine interfaces for the app.
  media::ScopedSLObjectItf recorder_object_;
  media::ScopedSLObjectItf engine_object_;

  SLRecordItf recorder_;

  // Buffer queue recorder interface.
  SLAndroidSimpleBufferQueueItf simple_buffer_queue_;

  SLAndroidDataFormat_PCM_EX format_;

  // Audio buffers that are allocated in the constructor based on
  // info from audio parameters.
  uint8_t* audio_data_[kMaxNumOfBuffersInQueue];

  int active_buffer_index_;
  int buffer_size_bytes_;

  bool started_;

  base::TimeDelta hardware_delay_;

  std::unique_ptr<media::AudioBus> audio_bus_;
};

}  // namespace media

#endif  // MEDIA_AUDIO_ANDROID_STARBOARD_AUDIO_INPUT_STREAM_H_
