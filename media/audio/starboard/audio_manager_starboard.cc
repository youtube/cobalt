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

#include "media/audio/starboard/audio_manager_starboard.h"

#include "media/audio/starboard/audio_input_stream_starboard.h"
#include "starboard/microphone.h"

namespace {
constexpr const char* kAudioManagerStarboardName = "AudioManagerStarboard";
constexpr int kDefaultSampleRate = 48'000;
}  // namespace

namespace media {

AudioManagerStarboard::AudioManagerStarboard(
    std::unique_ptr<AudioThread> audio_thread,
    AudioLogFactory* audio_log_factory)
    : AudioManagerBase(std::move(audio_thread), audio_log_factory) {}

AudioManagerStarboard::~AudioManagerStarboard() = default;

bool AudioManagerStarboard::HasAudioOutputDevices() {
  return false;
}

bool AudioManagerStarboard::HasAudioInputDevices() {
  return SbMicrophoneGetAvailable(/*out_info_array=*/nullptr,
                                  /*info_array_size=*/0) > 0;
}

void AudioManagerStarboard::GetAudioInputDeviceNames(
    AudioDeviceNames* device_names) {
  CHECK(device_names->empty());
  int microphone_count = SbMicrophoneGetAvailable(/*out_info_array=*/nullptr,
                                                  /*info_array_size=*/0);
  if (microphone_count <= 0) {
    return;
  }

  std::vector<SbMicrophoneInfo> infos(microphone_count);
  microphone_count = SbMicrophoneGetAvailable(infos.data(), infos.size());
  if (microphone_count <= 0) {
    return;
  }
  for (int i = 0; i < microphone_count; ++i) {
    device_names->emplace_back(infos[i].label, "default");
  }

  device_names->push_front(media::AudioDeviceName::CreateDefault());
}

void AudioManagerStarboard::GetAudioOutputDeviceNames(
    AudioDeviceNames* device_names) {
  NOTIMPLEMENTED();
}

AudioParameters AudioManagerStarboard::GetInputStreamParameters(
    const std::string& device_id) {
  // Chrome works best with 10ms buffers
  int buffer_size = kDefaultSampleRate / 100;

  return media::AudioParameters(media::AudioParameters::AUDIO_PCM_LOW_LATENCY,
                                ChannelLayoutConfig::Mono(), kDefaultSampleRate,
                                buffer_size);
}

const char* AudioManagerStarboard::GetName() {
  return kAudioManagerStarboardName;
}

AudioOutputStream* AudioManagerStarboard::MakeLinearOutputStream(
    const AudioParameters& params,
    const LogCallback& log_callback) {
  NOTIMPLEMENTED();
  return nullptr;
}

AudioOutputStream* AudioManagerStarboard::MakeLowLatencyOutputStream(
    const AudioParameters& params,
    const std::string& device_id,
    const LogCallback& log_callback) {
  return nullptr;
}

AudioInputStream* AudioManagerStarboard::MakeLinearInputStream(
    const AudioParameters& params,
    const std::string& device_id,
    const LogCallback& log_callback) {
  DCHECK_EQ(AudioParameters::AUDIO_PCM_LINEAR, params.format());
  return MakeInputStream(params, device_id);
}

AudioInputStream* AudioManagerStarboard::MakeLowLatencyInputStream(
    const AudioParameters& params,
    const std::string& device_id,
    const LogCallback& log_callback) {
  DCHECK_EQ(AudioParameters::AUDIO_PCM_LOW_LATENCY, params.format());
  return MakeInputStream(params, device_id);
}

AudioParameters AudioManagerStarboard::GetPreferredOutputStreamParameters(
    const std::string& output_device_id,
    const AudioParameters& input_params) {
  return AudioParameters();
}

AudioInputStream* AudioManagerStarboard::MakeInputStream(
    const AudioParameters& params,
    const std::string& device_id) {
  return new AudioInputStreamStarboard(this, params);
}

}  // namespace media
