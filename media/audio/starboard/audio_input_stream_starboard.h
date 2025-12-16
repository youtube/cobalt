// Copyright 2025 The Cobalt Authors. All Rights Reserved.
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

#include "base/synchronization/waitable_event.h"
#include "base/threading/thread.h"
#include "media/audio/audio_io.h"
#include "media/base/audio_parameters.h"
#include "starboard/microphone.h"

namespace media {

class AudioManagerStarboard;

class AudioInputStreamStarboard : public AudioInputStream {
 public:
  AudioInputStreamStarboard(AudioManagerStarboard* audio_manager,
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
  bool SetAutomaticGainControl(bool enabled) override;
  bool GetAutomaticGainControl() override;
  void SetOutputDeviceForAec(const std::string& output_device_id) override;

 private:
  const AudioParameters params_;
  base::Thread thread_;
  AudioInputCallback* callback_ = nullptr;
  base::WaitableEvent stop_event_;
  SbMicrophone microphone_ = kSbMicrophoneInvalid;

  void ReadAudio();
};

}  // namespace media

#endif  // MEDIA_AUDIO_STARBOARD_AUDIO_INPUT_STREAM_STARBOARD_H_
