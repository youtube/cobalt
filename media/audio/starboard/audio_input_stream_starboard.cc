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
    : params_(params), thread_("AudioInputStreamStarboard") {
  LOG(INFO) << "YO THOR - AudioInputStreamStarboard::AudioInputStreamStarboard"
            << " sample_rate: " << params_.sample_rate()
            << " channels: " << params_.channels()
            << " frames_per_buffer: " << params_.frames_per_buffer();
}

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
  LOG(INFO) << "YO THOR - AUDIO INPUT - START !";
  DCHECK(callback);
  DCHECK(!thread_.IsRunning());
  callback_ = callback;
  stop_event_.Reset();

  // We start reading data half |buffer_duration_| later than when the
  // buffer might have got filled, to accommodate some delays in the audio
  // driver. This could also give us a smooth read sequence going forward.
  base::TimeDelta initial_delay = params_.GetBufferDuration() / 2;
  next_read_time_ = base::TimeTicks::Now() + initial_delay;

  thread_.Start();
  base::TimeDelta delay = next_read_time_ - base::TimeTicks::Now();
  thread_.task_runner()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&AudioInputStreamStarboard::ReadAudio,
                     base::Unretained(this)),
      delay);
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

void AudioInputStreamStarboard::ReadAudio() {
  LOG(INFO) << "YO THOR - AUDIO INPUT READ AUDIO!";
  if (stop_event_.IsSignaled()) {
    return;
  }

  int available_frames = SbMicrophoneGetAvailableFrames(microphone_);
  if (available_frames < 0) {
    LOG(ERROR) << "SbMicrophoneGetAvailableFrames failed: " << available_frames;
    // An error occurred. Schedule a retry shortly to avoid busy-looping on
    // a permanent error.
    thread_.task_runner()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&AudioInputStreamStarboard::ReadAudio,
                       base::Unretained(this)),
        params_.GetBufferDuration() / 2);
    return;
  }

  if (available_frames < params_.frames_per_buffer()) {
    // Not enough data yet. Check again in a little bit.
    thread_.task_runner()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&AudioInputStreamStarboard::ReadAudio,
                       base::Unretained(this)),
        params_.GetBufferDuration() / 2);
    return;
  }

  // At least one full buffer is available. Read one buffer.
  auto audio_bus = AudioBus::Create(params_);
  const int buffer_size_frames = params_.frames_per_buffer();
  std::vector<int16_t> mono_buffer(buffer_size_frames);

  int bytes_read = SbMicrophoneRead(microphone_, mono_buffer.data(),
                                    buffer_size_frames * sizeof(int16_t));

  if (bytes_read > 0) {
    int frames_read = bytes_read / sizeof(int16_t);
    DCHECK_LE(frames_read, buffer_size_frames);
    LOG(INFO) << "YO THOR - Read " << bytes_read << " bytes for " << frames_read
              << " frames.";

    // If |frames_read| is less than |buffer_size_frames|, the remainder of
    // the audio_bus will be filled with silence.
    audio_bus->FromInterleaved<SignedInt16SampleTypeTraits>(mono_buffer.data(),
                                                            frames_read);

    LOG(INFO) << "YO THOR - Calling OnData with mono bus.";
    callback_->OnData(audio_bus.get(), base::TimeTicks::Now(), 0.0, {});
    LOG(INFO) << "YO THOR - Returned from OnData.";
  } else {
    LOG(WARNING) << "YO THOR SbMicrophoneRead returned " << bytes_read
                 << ". Dropping this buffer.";
  }

  // Schedule the next read.
  next_read_time_ += params_.GetBufferDuration();
  base::TimeTicks now = base::TimeTicks::Now();
  if (next_read_time_ < now) {
    // If we are behind, schedule the next read immediately.
    next_read_time_ = now;
  }

  base::TimeDelta delay = next_read_time_ - base::TimeTicks::Now();
  thread_.task_runner()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&AudioInputStreamStarboard::ReadAudio,
                     base::Unretained(this)),
      delay);
}

}  // namespace media
