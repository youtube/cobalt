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

#ifndef MEDIA_AUDIO_STARBOARD_AUDIO_INPUT_STREAM_STARBOARD_H_
#define MEDIA_AUDIO_STARBOARD_AUDIO_INPUT_STREAM_STARBOARD_H_

#include "base/memory/raw_ptr.h"
#include "base/threading/thread.h"
#include "media/audio/agc_audio_stream.h"
#include "media/audio/audio_io.h"
#include "media/base/audio_parameters.h"
#include "starboard/microphone.h"

namespace media {

class AudioManagerBase;

// We request a buffer twice the size of a single read to allow for double-
// buffering in the underlying microphone implementation.
constexpr int kMicrophoneBufferSizeMultiplier = 2;

class AudioInputStreamStarboard : public AgcAudioStream<AudioInputStream> {
 public:
  AudioInputStreamStarboard(AudioManagerBase* audio_manager,
                            const AudioParameters& params);
  ~AudioInputStreamStarboard() override;

  // AudioInputStream implementation.
  OpenOutcome Open() override;
  void Start(AudioInputCallback* callback) override;
  void Stop() override;
  void Close() override;
  double GetMaxVolume() override;
  void SetVolume(double volume) override;
  double GetVolume() override;
  bool IsMuted() override;
  void SetOutputDeviceForAec(const std::string& output_device_id) override;

 private:
  // Logs the error and invokes any registered callbacks.
  void HandleError(const char* method);

  // Reads one or more buffers of audio from the device, passes on to the
  // registered callback and schedules the next read.
  void ReadAudio();

  // Set |running_| to false on |capture_thread_|.
  void StopRunningOnCaptureThread();

  // Non-refcounted pointer back to the audio manager.
  // The AudioManager indirectly holds on to stream objects, so we don't
  // want circular references.  Additionally, stream objects live on the audio
  // thread, which is owned by the audio manager and we don't want to addref
  // the manager from that thread.
  const raw_ptr<AudioManagerBase> audio_manager_;
  const AudioParameters params_;
  base::TimeDelta buffer_duration_;  // Length of each recorded buffer.
  raw_ptr<AudioInputCallback> callback_ = nullptr;
  base::TimeTicks next_read_time_;  // Scheduled time for next read callback.
  base::Thread capture_thread_;
  bool running_{false};
  SbMicrophone microphone_ = kSbMicrophoneInvalid;
  bool closing_ = false;
  std::unique_ptr<AudioBus> audio_bus_;
  std::vector<int16_t> buffer_;
};

}  // namespace media

#endif  // MEDIA_AUDIO_STARBOARD_AUDIO_INPUT_STREAM_STARBOARD_H_
