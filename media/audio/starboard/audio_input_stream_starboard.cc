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
    LOG(ERROR) << "AudioInputStreamStarboard::Open() - No mics";
    return OpenOutcome::kFailed;
  }
  int min_read_size = info.min_read_size;

  const int requested_bytes =
      params_.GetBytesPerBuffer(media::SampleFormat::kSampleFormatS16);

  int mic_buffer_size_bytes = requested_bytes * kMicrophoneBufferSizeMultiplier;
  if (min_read_size > 0) {
    mic_buffer_size_bytes = std::max(
        mic_buffer_size_bytes, min_read_size * kMicrophoneBufferSizeMultiplier);
  }

  LOG(INFO) << "AudioInputStreamStarboard::Open() - Creating mic with rate="
            << params_.sample_rate() << ", channels=" << params_.channels()
            << ", buffer_size=" << mic_buffer_size_bytes;
  microphone_ =
      SbMicrophoneCreate(info.id, params_.sample_rate(), mic_buffer_size_bytes);

  if (!SbMicrophoneIsValid(microphone_)) {
    LOG(ERROR) << "AudioInputStreamStarboard::Open() - Mic invalid";
    microphone_ = kSbMicrophoneInvalid;
    return OpenOutcome::kFailed;
  }

  read_size_in_bytes_ = std::max(requested_bytes, min_read_size);
  temp_buffer_.resize(read_size_in_bytes_ / sizeof(int16_t));

  int temp_audio_bus_frames = temp_buffer_.size() / params_.channels();
  temp_audio_bus_ = AudioBus::Create(params_.channels(), temp_audio_bus_frames);

  int fifo_capacity_frames =
      params_.frames_per_buffer() * 2 + temp_audio_bus_frames;
  fifo_ = std::make_unique<AudioFifo>(params_.channels(), fifo_capacity_frames);

  audio_bus_ = AudioBus::Create(params_);

  LOG(INFO) << "AudioInputStreamStarboard::Open() - Success. "
            << "min_read_size=" << min_read_size
            << ", requested_bytes=" << requested_bytes
            << ", read_size_in_bytes=" << read_size_in_bytes_
            << ", temp_buffer_samples=" << temp_buffer_.size();
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
  CHECK(capture_thread_.task_runner()->BelongsToCurrentThread());
  CHECK(callback_);
  if (!running_) {
    return;
  }

  int bytes_read =
      SbMicrophoneRead(microphone_, temp_buffer_.data(), read_size_in_bytes_);

  LOG(INFO) << "SbMicrophoneRead requested=" << read_size_in_bytes_
            << " returned=" << bytes_read;

  if (bytes_read < 0) {
    DLOG(WARNING) << "SbMicrophoneRead returned " << bytes_read;
    HandleError("SbMicrophoneRead");
    return;
  }

  if (bytes_read > 0) {
    int frames_read = bytes_read / (params_.channels() * sizeof(int16_t));
    temp_audio_bus_->FromInterleaved<SignedInt16SampleTypeTraits>(
        temp_buffer_.data(), frames_read);
    fifo_->Push(temp_audio_bus_.get(), frames_read);
    LOG(INFO) << "FIFO pushed " << frames_read << " frames, total now "
              << fifo_->frames();
  }

  if (fifo_->frames() >= params_.frames_per_buffer()) {
    fifo_->Consume(audio_bus_.get(), 0, params_.frames_per_buffer());
    LOG(INFO) << "FIFO consumed " << params_.frames_per_buffer()
              << " frames, left " << fifo_->frames();
    callback_->OnData(audio_bus_.get(), base::TimeTicks::Now(), 1.0, {});

    next_read_time_ =
        std::max(base::TimeTicks::Now(), next_read_time_ + buffer_duration_);
    capture_thread_.task_runner()->PostDelayedTaskAt(
        base::subtle::PostDelayedTaskPassKey(), FROM_HERE,
        base::BindOnce(&AudioInputStreamStarboard::ReadAudio,
                       base::Unretained(this)),
        next_read_time_, base::subtle::DelayPolicy::kPrecise);
  } else {
    // Not enough data yet. Check again soon.
    LOG(INFO) << "FIFO only has " << fifo_->frames()
              << " frames, waiting for more (need "
              << params_.frames_per_buffer() << ")";
    base::TimeTicks next_check_time =
        base::TimeTicks::Now() + buffer_duration_ / 2;
    capture_thread_.task_runner()->PostDelayedTaskAt(
        base::subtle::PostDelayedTaskPassKey(), FROM_HERE,
        base::BindOnce(&AudioInputStreamStarboard::ReadAudio,
                       base::Unretained(this)),
        next_check_time, base::subtle::DelayPolicy::kPrecise);
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
