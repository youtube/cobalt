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

#include "media/audio/starboard/audio_input_stream_starboard.h"

#include "base/logging.h"
#include "media/audio/audio_manager.h"
#include "media/audio/audio_manager_base.h"

namespace media {

AudioInputStreamStarboard::AudioInputStreamStarboard(
    AudioManagerBase* audio_manager,
    const AudioParameters& params)
    : audio_manager_(audio_manager),
      params_(params),
      buffer_duration_(base::Microseconds(
          params.frames_per_buffer() * base::Time::kMicrosecondsPerSecond /
          static_cast<float>(params.sample_rate()))),
      capture_thread_("AudioInputStreamStarboard") {}

AudioInputStreamStarboard::~AudioInputStreamStarboard() {
  Close();
}

AudioInputStream::OpenOutcome AudioInputStreamStarboard::Open() {
  if (microphone_) {
    return OpenOutcome::kAlreadyOpen;
  }

  SbMicrophoneInfo info;

  if (SbMicrophoneGetAvailable(&info, 1) < 1) {
    return OpenOutcome::kFailed;
  }

  microphone_ = SbMicrophoneCreate(
      info.id, params_.sample_rate(),
      params_.frames_per_buffer() * kMicrophoneBufferSizeMultiplier);

  if (!SbMicrophoneIsValid(microphone_)) {
    microphone_ = kSbMicrophoneInvalid;
    return OpenOutcome::kFailed;
  }

  audio_bus_ = AudioBus::Create(params_);
  buffer_.resize(params_.frames_per_buffer() * params_.channels());

  return OpenOutcome::kSuccess;
}

void AudioInputStreamStarboard::Start(AudioInputCallback* callback) {
  DCHECK(!callback_ && callback);
  callback_ = callback;
  if (!SbMicrophoneOpen(microphone_)) {
    HandleError("SbMicrophoneOpen");
    return;
  }
  CHECK(capture_thread_.StartWithOptions(
      base::Thread::Options(base::ThreadType::kRealtimeAudio)));
  // We start reading data half |buffer_duration_| later than when the
  // buffer might have got filled, to accommodate some delays in the audio
  // driver. This could also give us a smooth read sequence going forward.
  base::TimeDelta delay = buffer_duration_ + buffer_duration_ / 2;
  next_read_time_ = base::TimeTicks::Now() + delay;
  running_ = true;
  StartAgc();
  capture_thread_.task_runner()->PostDelayedTaskAt(
      base::subtle::PostDelayedTaskPassKey(), FROM_HERE,
      base::BindOnce(&AudioInputStreamStarboard::ReadAudio,
                     base::Unretained(this)),
      next_read_time_, base::subtle::DelayPolicy::kPrecise);
}

void AudioInputStreamStarboard::Stop() {
  StopAgc();
  StopRunningOnCaptureThread();
  capture_thread_.Stop();
}

void AudioInputStreamStarboard::Close() {
  if (closing_) {
    return;
  }
  closing_ = true;
  Stop();  // Stop the capture thread first.
  if (SbMicrophoneIsValid(microphone_)) {
    SbMicrophoneDestroy(microphone_);
    microphone_ = kSbMicrophoneInvalid;
  }
  audio_manager_->ReleaseInputStream(this);
}

double AudioInputStreamStarboard::GetMaxVolume() {
  return 1.0;
}

void AudioInputStreamStarboard::SetVolume(double volume) {
  NOTIMPLEMENTED();
}

double AudioInputStreamStarboard::GetVolume() {
  NOTIMPLEMENTED();
  return 1.0;
}

bool AudioInputStreamStarboard::IsMuted() {
  NOTIMPLEMENTED();
  return false;
}

void AudioInputStreamStarboard::SetOutputDeviceForAec(
    const std::string& output_device_id) {
  NOTIMPLEMENTED();
}

void AudioInputStreamStarboard::HandleError(const char* method) {
  LOG(WARNING) << "Error in: " << method;
  if (callback_) {
    callback_->OnError();
  }
}

void AudioInputStreamStarboard::ReadAudio() {
  DCHECK(capture_thread_.task_runner()->BelongsToCurrentThread());
  DCHECK(callback_);
  if (!running_) {
    return;
  }

  const int buffer_size_bytes =
      params_.frames_per_buffer() * params_.channels() * sizeof(int16_t);

  int bytes_read =
      SbMicrophoneRead(microphone_, buffer_.data(), buffer_size_bytes);

  if (bytes_read > 0) {
    int frames_read = bytes_read / (params_.channels() * sizeof(int16_t));
    DCHECK_LE(frames_read, params_.frames_per_buffer());

    audio_bus_->FromInterleaved<SignedInt16SampleTypeTraits>(buffer_.data(),
                                                             frames_read);

    callback_->OnData(audio_bus_.get(), base::TimeTicks::Now(), 1.0, {});

    // Schedule the next read based on an absolute deadline.
    next_read_time_ += buffer_duration_;
    base::TimeTicks now = base::TimeTicks::Now();
    if (next_read_time_ < now) {
      // The read callback is behind schedule. Schedule the next one to run
      // immediately to catch up.
      next_read_time_ = now;
    }
    capture_thread_.task_runner()->PostDelayedTaskAt(
        base::subtle::PostDelayedTaskPassKey(), FROM_HERE,
        base::BindOnce(&AudioInputStreamStarboard::ReadAudio,
                       base::Unretained(this)),
        next_read_time_, base::subtle::DelayPolicy::kPrecise);

  } else if (bytes_read == 0) {
    // No data available yet. Check again in a little bit.
    base::TimeTicks next_check_time =
        base::TimeTicks::Now() + buffer_duration_ / 2;
    capture_thread_.task_runner()->PostDelayedTaskAt(
        base::subtle::PostDelayedTaskPassKey(), FROM_HERE,
        base::BindOnce(&AudioInputStreamStarboard::ReadAudio,
                       base::Unretained(this)),
        next_check_time, base::subtle::DelayPolicy::kPrecise);
  } else {
    DLOG(WARNING) << "SbMicrophoneRead returned " << bytes_read
                  << ". Dropping this buffer.";
    HandleError("SbMicrophoneRead");
    return;
  }
}

void AudioInputStreamStarboard::StopRunningOnCaptureThread() {
  if (!capture_thread_.IsRunning()) {
    return;
  }
  if (!capture_thread_.task_runner()->BelongsToCurrentThread()) {
    capture_thread_.task_runner()->PostTask(
        FROM_HERE,
        base::BindOnce(&AudioInputStreamStarboard::StopRunningOnCaptureThread,
                       base::Unretained(this)));
    return;
  }
  running_ = false;
}

}  // namespace media
