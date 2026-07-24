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

#include <algorithm>

#include "base/logging.h"
#include "media/audio/audio_manager.h"
#include "media/audio/audio_manager_base.h"

namespace media {

namespace {
constexpr int kMinReadBytes = 2048;
}

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
    LOG(ERROR) << "AudioInputStreamStarboard::Open() - No mics";
    return OpenOutcome::kFailed;
  }

  const int buffer_size_bytes =
      params_.GetBytesPerBuffer(media::SampleFormat::kSampleFormatS16) *
      kMicrophoneBufferSizeMultiplier;

  LOG(INFO) << "AudioInputStreamStarboard::Open() - Creating mic with rate="
            << params_.sample_rate() << ", channels=" << params_.channels()
            << ", buffer_size=" << buffer_size_bytes;
  microphone_ =
      SbMicrophoneCreate(info.id, params_.sample_rate(), buffer_size_bytes);

  if (!SbMicrophoneIsValid(microphone_)) {
    LOG(ERROR) << "AudioInputStreamStarboard::Open() - Mic invalid";
    microphone_ = kSbMicrophoneInvalid;
    return OpenOutcome::kFailed;
  }

  audio_bus_ = AudioBus::Create(params_);
  buffer_.resize(params_.frames_per_buffer() * params_.channels());

  LOG(INFO) << "AudioInputStreamStarboard::Open() - Success";
  return OpenOutcome::kSuccess;
}

void AudioInputStreamStarboard::Start(AudioInputCallback* callback) {
  LOG(INFO) << "AudioInputStreamStarboard::Start()";
  CHECK(!callback_ && callback);
  callback_ = callback;
  if (!SbMicrophoneOpen(microphone_)) {
    LOG(ERROR) << "SbMicrophoneOpen Failed";
    HandleError("SbMicrophoneOpen");
    return;
  }
  LOG(INFO) << "SbMicrophoneOpen Succeeded";

  LOG(INFO) << "AudioInputStreamStarboard::Start() - Starting thread";
  base::Thread::Options options(base::ThreadType::kRealtimeAudio);
  if (!capture_thread_.StartWithOptions(std::move(options))) {
    HandleError("capture_thread_.StartWithOptions");
    return;
  }
  LOG(INFO) << "Capture thread started";

  // We start reading data half |buffer_duration_| later than when the
  // buffer might have got filled, to accommodate some delays in the audio
  // driver. This could also give us a smooth read sequence going forward.
  base::TimeDelta delay = buffer_duration_ + buffer_duration_ / 2;
  next_read_time_ = base::TimeTicks::Now() + delay;
  running_ = true;
  StartAgc();

  bool posted = capture_thread_.task_runner()->PostDelayedTaskAt(
      base::subtle::PostDelayedTaskPassKey(), FROM_HERE,
      base::BindOnce(&AudioInputStreamStarboard::ReadAudio,
                     base::Unretained(this)),
      next_read_time_, base::subtle::DelayPolicy::kPrecise);

  if (!posted) {
    HandleError("PostDelayedTaskAt");
    running_ = false;
    capture_thread_.Stop();
    return;
  }
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
  // Starboard doesn't allow volume changes
  NOTIMPLEMENTED();
}

double AudioInputStreamStarboard::GetVolume() {
  // Starboard doesn't allow volume changes
  NOTIMPLEMENTED();
  return 1.0;
}

bool AudioInputStreamStarboard::IsMuted() {
  // Starboard doesn't allow volume changes
  NOTIMPLEMENTED();
  return false;
}

void AudioInputStreamStarboard::SetOutputDeviceForAec(
    const std::string& output_device_id) {
  // starboard does not support output device
  NOTIMPLEMENTED();
}

void AudioInputStreamStarboard::HandleError(const char* method) {
  LOG(WARNING) << "Error in: " << method;
  if (callback_) {
    callback_->OnError();
  }
}

void AudioInputStreamStarboard::ReadAudio() {
  CHECK(capture_thread_.task_runner()->BelongsToCurrentThread());
  CHECK(callback_);
  if (!running_) {
    return;
  }

  const size_t target_buffer_size_bytes =
      params_.frames_per_buffer() * params_.channels() * sizeof(int16_t);
  const size_t max_fifo_size_bytes = target_buffer_size_bytes * kMaxFifoBuffers;
  const size_t read_size_bytes =
      std::max(static_cast<size_t>(kMinReadBytes), target_buffer_size_bytes);

  // Read directly into the FIFO to avoid extra allocation and copy.
  size_t current_fifo_size = audio_fifo_.size();
  audio_fifo_.resize(current_fifo_size + read_size_bytes);
  int bytes_read = SbMicrophoneRead(
      microphone_,
      reinterpret_cast<char*>(audio_fifo_.data() + current_fifo_size),
      read_size_bytes);

  if (bytes_read > 0) {
    audio_fifo_.resize(current_fifo_size + bytes_read);
    if (audio_fifo_.size() > max_fifo_size_bytes) {
      size_t bytes_to_drop = audio_fifo_.size() - max_fifo_size_bytes;
      audio_fifo_.erase(audio_fifo_.begin(),
                        audio_fifo_.begin() + bytes_to_drop);
      DLOG(WARNING) << "Audio FIFO overflow, dropped " << bytes_to_drop
                    << " bytes.";
    }
  } else {
    audio_fifo_.resize(current_fifo_size);
    if (bytes_read < 0) {
      DLOG(WARNING) << "SbMicrophoneRead returned " << bytes_read << ".";
      HandleError("SbMicrophoneRead");
      return;
    }
  }

  bool data_pushed = false;
  // Process all available complete buffers to minimize latency.
  while (audio_fifo_.size() >= target_buffer_size_bytes) {
    std::copy(audio_fifo_.begin(),
              audio_fifo_.begin() + target_buffer_size_bytes,
              reinterpret_cast<uint8_t*>(buffer_.data()));
    audio_fifo_.erase(audio_fifo_.begin(),
                      audio_fifo_.begin() + target_buffer_size_bytes);

    audio_bus_->FromInterleaved<SignedInt16SampleTypeTraits>(
        buffer_.data(), params_.frames_per_buffer());

    callback_->OnData(audio_bus_.get(), base::TimeTicks::Now(), 1.0, {});
    data_pushed = true;
    next_read_time_ =
        std::max(base::TimeTicks::Now(), next_read_time_ + buffer_duration_);
  }

  if (!data_pushed) {
    next_read_time_ = base::TimeTicks::Now() + buffer_duration_ / 2;
  }

  capture_thread_.task_runner()->PostDelayedTaskAt(
      base::subtle::PostDelayedTaskPassKey(), FROM_HERE,
      base::BindOnce(&AudioInputStreamStarboard::ReadAudio,
                     base::Unretained(this)),
      next_read_time_, base::subtle::DelayPolicy::kPrecise);
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
