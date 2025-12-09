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

#include "media/audio/starboard/audio_input_stream_starboard.h"

#include "base/logging.h"
#include "starboard/microphone.h"

namespace media {

AudioInputStreamStarboard::AudioInputStreamStarboard(
    AudioManagerStarboard* audio_manager,
    const AudioParameters& params)
    : params_(params), thread_("AudioInputStreamStarboard") {}

AudioInputStreamStarboard::~AudioInputStreamStarboard() {
  Close();
}

AudioInputStream::OpenOutcome AudioInputStreamStarboard::Open() {
  LOG(INFO) << "YO THOR - AudioInputStreamStarboard::Open";
  if (microphone_) {
    LOG(INFO) << "YO THOR - AudioInputStreamStarboard::Open - Already open";
    return OpenOutcome::kAlreadyOpen;
  }

  SbMicrophoneInfo info;

  if (SbMicrophoneGetAvailable(&info, 1) < 1) {
    LOG(INFO) << "YO THOR - AudioInputStreamStarboard::Open - "
                 "SbMicrophoneGetAvailable failed";
    return OpenOutcome::kFailed;
  }

  microphone_ = SbMicrophoneCreate(info.id, params_.sample_rate(),
                                   params_.frames_per_buffer() * 2);

  if (!SbMicrophoneIsValid(microphone_)) {
    LOG(INFO) << "YO THOR - AudioInputStreamStarboard::Open - "
                 "SbMicrophoneCreate failed. Sample rate: "
              << params_.sample_rate();
    return OpenOutcome::kFailed;
  }

  if (!SbMicrophoneOpen(microphone_)) {
    LOG(INFO) << "YO THOR - AudioInputStreamStarboard::Open - SbMicrophoneOpen "
                 "failed";
    SbMicrophoneDestroy(microphone_);
    microphone_ = kSbMicrophoneInvalid;
    return OpenOutcome::kFailed;
  }

  return OpenOutcome::kSuccess;
}

void AudioInputStreamStarboard::Start(AudioInputCallback* callback) {
  DCHECK(callback);
  DCHECK(!thread_.IsRunning());
  callback_ = callback;
  stop_event_.Reset();
  thread_.Start();
  thread_.task_runner()->PostTask(
      FROM_HERE, base::BindOnce(&AudioInputStreamStarboard::ThreadMain,
                                base::Unretained(this)));
}

void AudioInputStreamStarboard::Stop() {
  if (thread_.IsRunning()) {
    stop_event_.Signal();
    thread_.Stop();
  }
  callback_ = nullptr;
}

void AudioInputStreamStarboard::Close() {
  Stop();

  if (SbMicrophoneIsValid(microphone_)) {
    SbMicrophoneClose(microphone_);
    SbMicrophoneDestroy(microphone_);
    microphone_ = kSbMicrophoneInvalid;
  }
}

double AudioInputStreamStarboard::GetMaxVolume() {
  return 1.0;
}

void AudioInputStreamStarboard::SetVolume(double volume) {}

double AudioInputStreamStarboard::GetVolume() {
  return 1.0;
}

bool AudioInputStreamStarboard::IsMuted() {
  return false;
}

bool AudioInputStreamStarboard::SetAutomaticGainControl(bool enabled) {
  return false;
}

bool AudioInputStreamStarboard::GetAutomaticGainControl() {
  return false;
}

void AudioInputStreamStarboard::SetOutputDeviceForAec(
    const std::string& output_device_id) {}

void AudioInputStreamStarboard::ThreadMain() {
  auto audio_bus = AudioBus::Create(params_);

  // The read buffer is for mono data from the microphone.
  const int buffer_size_frames = params_.frames_per_buffer();
  std::vector<int16_t> mono_buffer(buffer_size_frames);

  while (!stop_event_.IsSignaled()) {
    // Read mono data. The size is frames * 1 channel * 2 bytes/sample.
    int bytes_read = SbMicrophoneRead(microphone_, mono_buffer.data(),
                                      mono_buffer.size() * sizeof(int16_t));

    if (bytes_read > 0) {
      int frames = bytes_read / sizeof(int16_t);
      DCHECK_LE(frames, buffer_size_frames);
      LOG(INFO) << "YO THOR - Read " << bytes_read << " bytes for " << frames
                << " frames.";

      // Create a temporary stereo buffer on the stack and duplicate the mono
      // samples.
      std::vector<int16_t> stereo_buffer(frames * 2);
      for (int i = 0; i < frames; ++i) {
        stereo_buffer[i * 2] = mono_buffer[i];
        stereo_buffer[i * 2 + 1] = mono_buffer[i];
      }

      // Convert the stereo PCM data into the planar float format of the
      // AudioBus.
      audio_bus->FromInterleaved<SignedInt16SampleTypeTraits>(
          stereo_buffer.data(), frames);

      LOG(INFO) << "YO THOR - Calling OnData with stereo bus.";
      callback_->OnData(audio_bus.get(), base::TimeTicks::Now(), 0.0, {});
      LOG(INFO) << "YO THOR - Returned from OnData.";
    }
  }
}

}  // namespace media
