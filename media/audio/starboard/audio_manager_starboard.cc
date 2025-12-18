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

#include "base/debug/stack_trace.h"

#include "base/logging.h"
#include "base/time/time.h"
#include "media/audio/starboard/audio_input_stream_starboard.h"
#include "starboard/microphone.h"

namespace media {

AudioManagerStarboard::AudioManagerStarboard(
    std::unique_ptr<AudioThread> audio_thread,
    AudioLogFactory* audio_log_factory)
    : AudioManagerBase(std::move(audio_thread), audio_log_factory) {}

AudioManagerStarboard::~AudioManagerStarboard() = default;

bool AudioManagerStarboard::HasAudioOutputDevices() {
  // TODO(b/294816013): Implement this.
  return true;
}

bool AudioManagerStarboard::HasAudioInputDevices() {
  return SbMicrophoneGetAvailable(nullptr, 0) > 0;
}

void AudioManagerStarboard::GetAudioInputDeviceNames(
    AudioDeviceNames* device_names) {
  base::AutoLock auto_lock(cache_lock_);
  if (base::TimeTicks::Now() - last_input_device_names_cache_update_ <
      kCacheDeviceListsTimeout) {
    LOG(INFO) << "YO THOR - AudioManagerStarboard::GetAudioInputDeviceNames() "
                 "- CACHE HIT";
    *device_names = input_device_names_cache_;
    return;
  }

  LOG(INFO) << "YO THOR - AudioManagerStarboard::GetAudioInputDeviceNames() - "
               "CACHE MISS";
  base::debug::StackTrace st;
  st.Print();

  base::TimeTicks start_time = base::TimeTicks::Now();

  // Clear the cache before populating it.
  input_device_names_cache_.clear();

  base::TimeTicks sb_get_available_count_start = base::TimeTicks::Now();
  int microphone_count = SbMicrophoneGetAvailable(nullptr, 0);
  LOG(INFO) << "YO THOR - SbMicrophoneGetAvailable (count) took "
            << (base::TimeTicks::Now() - sb_get_available_count_start)
                   .InMilliseconds()
            << " ms. Count: " << microphone_count;

  if (microphone_count <= 0) {
    LOG(INFO) << "YO THOR - AudioManagerStarboard::GetAudioInputDeviceNames() "
                 "finished in "
              << (base::TimeTicks::Now() - start_time).InMilliseconds()
              << " ms.";
    last_input_device_names_cache_update_ = base::TimeTicks::Now();
    *device_names = input_device_names_cache_;
    return;
  }

  std::vector<SbMicrophoneInfo> infos(microphone_count);
  base::TimeTicks sb_get_available_info_start = base::TimeTicks::Now();
  microphone_count = SbMicrophoneGetAvailable(infos.data(), infos.size());
  LOG(INFO)
      << "YO THOR - SbMicrophoneGetAvailable (info) took "
      << (base::TimeTicks::Now() - sb_get_available_info_start).InMilliseconds()
      << " ms. Actual count: " << microphone_count;

  if (microphone_count <= 0) {
    LOG(INFO) << "YO THOR - AudioManagerStarboard::GetAudioInputDeviceNames() "
                 "finished in "
              << (base::TimeTicks::Now() - start_time).InMilliseconds()
              << " ms.";
    last_input_device_names_cache_update_ = base::TimeTicks::Now();
    *device_names = input_device_names_cache_;
    return;
  }

  for (int i = 0; i < microphone_count; ++i) {
    input_device_names_cache_.emplace_back(infos[i].label, "default");
  }

  LOG(INFO) << "YO THOR - AudioManagerStarboard::GetAudioInputDeviceNames() "
               "finished in "
            << (base::TimeTicks::Now() - start_time).InMilliseconds() << " ms.";

  last_input_device_names_cache_update_ = base::TimeTicks::Now();
  *device_names = input_device_names_cache_;
}

void AudioManagerStarboard::GetAudioOutputDeviceNames(
    AudioDeviceNames* device_names) {
  // TODO(b/294816013): Implement this.
}

AudioOutputStream* AudioManagerStarboard::MakeAudioOutputStream(
    const AudioParameters& params,
    const std::string& device_id,
    const LogCallback& log_callback) {
  // TODO(b/294816013): Implement this.
  return nullptr;
}

AudioInputStream* AudioManagerStarboard::MakeAudioInputStream(
    const AudioParameters& params,
    const std::string& device_id,
    const LogCallback& log_callback) {
  return new AudioInputStreamStarboard(this, params);
}

const char* AudioManagerStarboard::GetName() {
  return "Starboard";
}

void AudioManagerStarboard::ShutdownOnAudioThread() {
  AudioManagerBase::ShutdownOnAudioThread();
}

AudioParameters AudioManagerStarboard::GetInputStreamParameters(
    const std::string& device_id) {
  // TODO(b/294816013): Get the device info and check for sample rate support.
  AudioParameters params(AudioParameters::AUDIO_PCM_LOW_LATENCY,
                         ChannelLayoutConfig::Mono(), 48000, 480);
  return params;
}

AudioParameters AudioManagerStarboard::GetPreferredOutputStreamParameters(
    const std::string& output_device_id,
    const AudioParameters& input_params) {
  // TODO(b/294816013): Implement this.
  return AudioParameters();
}

AudioOutputStream* AudioManagerStarboard::MakeLinearOutputStream(
    const AudioParameters& params,
    const LogCallback& log_callback) {
  DCHECK_EQ(params.format(), AudioParameters::AUDIO_PCM_LINEAR);
  // TODO(b/294816013): Implement this.
  return nullptr;
}

AudioOutputStream* AudioManagerStarboard::MakeLowLatencyOutputStream(
    const AudioParameters& params,
    const std::string& device_id,
    const LogCallback& log_callback) {
  DCHECK_EQ(params.format(), AudioParameters::AUDIO_PCM_LOW_LATENCY);
  // TODO(b/294816013): Implement this.
  return nullptr;
}

AudioInputStream* AudioManagerStarboard::MakeLinearInputStream(
    const AudioParameters& params,
    const std::string& device_id,
    const LogCallback& log_callback) {
  DCHECK_EQ(params.format(), AudioParameters::AUDIO_PCM_LINEAR);
  return MakeAudioInputStream(params, device_id, log_callback);
}

AudioInputStream* AudioManagerStarboard::MakeLowLatencyInputStream(
    const AudioParameters& params,
    const std::string& device_id,
    const LogCallback& log_callback) {
  DCHECK_EQ(params.format(), AudioParameters::AUDIO_PCM_LOW_LATENCY);
  return MakeAudioInputStream(params, device_id, log_callback);
}

}  // namespace media
