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

#ifndef MEDIA_AUDIO_STARBOARD_AUDIO_MANAGER_STARBOARD_H_
#define MEDIA_AUDIO_STARBOARD_AUDIO_MANAGER_STARBOARD_H_

#include "media/audio/audio_manager_base.h"

namespace media {

class AudioManagerStarboard : public AudioManagerBase {
 public:
  AudioManagerStarboard(std::unique_ptr<AudioThread> audio_thread,
                        AudioLogFactory* audio_log_factory);

  AudioManagerStarboard(const AudioManagerStarboard&) = delete;
  AudioManagerStarboard& operator=(const AudioManagerStarboard&) = delete;

  ~AudioManagerStarboard() override;

  // AudioManager implementation.
  bool HasAudioOutputDevices() override;
  bool HasAudioInputDevices() override;
  void GetAudioInputDeviceNames(AudioDeviceNames* device_names) override;
  void GetAudioOutputDeviceNames(AudioDeviceNames* device_names) override;
  AudioOutputStream* MakeAudioOutputStream(
      const AudioParameters& params,
      const std::string& device_id,
      const LogCallback& log_callback) override;
  AudioInputStream* MakeAudioInputStream(
      const AudioParameters& params,
      const std::string& device_id,
      const LogCallback& log_callback) override;
  const char* GetName() override;

 protected:
  void ShutdownOnAudioThread() override;
  AudioParameters GetInputStreamParameters(
      const std::string& device_id) override;
  AudioParameters GetPreferredOutputStreamParameters(
      const std::string& output_device_id,
      const AudioParameters& input_params) override;

 private:
  // AudioManagerBase implementation.
  AudioOutputStream* MakeLinearOutputStream(
      const AudioParameters& params,
      const LogCallback& log_callback) override;
  AudioOutputStream* MakeLowLatencyOutputStream(
      const AudioParameters& params,
      const std::string& device_id,
      const LogCallback& log_callback) override;
  AudioInputStream* MakeLinearInputStream(
      const AudioParameters& params,
      const std::string& device_id,
      const LogCallback& log_callback) override;
  AudioInputStream* MakeLowLatencyInputStream(
      const AudioParameters& params,
      const std::string& device_id,
      const LogCallback& log_callback) override;
};

}  // namespace media

#endif  // MEDIA_AUDIO_STARBOARD_AUDIO_MANAGER_STARBOARD_H_
