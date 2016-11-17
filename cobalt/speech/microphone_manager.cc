/*
 * Copyright 2016 Google Inc. All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "cobalt/speech/microphone_manager.h"

#if defined(ENABLE_FAKE_MICROPHONE)
#include "cobalt/speech/microphone_fake.h"
#endif  // defined(ENABLE_FAKE_MICROPHONE)
#if defined(SB_USE_SB_MICROPHONE)
#include "cobalt/speech/microphone_starboard.h"
#endif  // defined(SB_USE_SB_MICROPHONE)
#include "cobalt/speech/speech_recognition_error.h"

namespace cobalt {
namespace speech {

namespace {
// Size of an audio buffer.
const int kBufferSizeInBytes = 8 * 1024;
// The frequency which we read the data from devices.
const float kMicReadRateInHertz = 60.0f;
}  // namespace

MicrophoneManager::MicrophoneManager(int sample_rate,
                                     const DataReceivedCallback& data_received,
                                     const CompletionCallback& completion,
                                     const ErrorCallback& error,
                                     const Microphone::Options& options)
    : sample_rate_(sample_rate),
      data_received_callback_(data_received),
      completion_callback_(completion),
      error_callback_(error),
#if defined(ENABLE_FAKE_MICROPHONE)
      microphone_options_(options),
#endif  // defined(ENABLE_FAKE_MICROPHONE)
      state_(kStopped),
      thread_("microphone_thread") {
  UNREFERENCED_PARAMETER(sample_rate_);
#if defined(ENABLE_FAKE_MICROPHONE)
  UNREFERENCED_PARAMETER(microphone_options_);
#else
  UNREFERENCED_PARAMETER(options);
#endif  // defined(ENABLE_FAKE_MICROPHONE)
  thread_.StartWithOptions(base::Thread::Options(MessageLoop::TYPE_IO, 0));
}

MicrophoneManager::~MicrophoneManager() {
  thread_.message_loop()->PostTask(
      FROM_HERE,
      base::Bind(&MicrophoneManager::DestroyInternal, base::Unretained(this)));
}

void MicrophoneManager::Open() {
  thread_.message_loop()->PostTask(
      FROM_HERE,
      base::Bind(&MicrophoneManager::OpenInternal, base::Unretained(this)));
}

void MicrophoneManager::Close() {
  thread_.message_loop()->PostTask(
      FROM_HERE,
      base::Bind(&MicrophoneManager::CloseInternal, base::Unretained(this)));
}

bool MicrophoneManager::CreateIfNecessary() {
  DCHECK(thread_.message_loop_proxy()->BelongsToCurrentThread());

  if (microphone_) {
    return true;
  }

#if defined(SB_USE_SB_MICROPHONE)
#if defined(ENABLE_FAKE_MICROPHONE)
  if (microphone_options_.enable_fake_microphone) {
    microphone_.reset(new MicrophoneFake(microphone_options_));
  } else {
    microphone_.reset(
        new MicrophoneStarboard(sample_rate_, kBufferSizeInBytes));
  }
#else
  microphone_.reset(new MicrophoneStarboard(sample_rate_, kBufferSizeInBytes));
#endif  // defined(ENABLE_FAKE_MICROPHONE)
#endif  // defined(SB_USE_SB_MICROPHONE)

  if (microphone_ && microphone_->IsValid()) {
    state_ = kStopped;
    return true;
  } else {
    DLOG(WARNING) << "Microphone creation failed.";
    microphone_.reset();
    state_ = kError;
    error_callback_.Run(new SpeechRecognitionError(
        SpeechRecognitionError::kAudioCapture, "No microphone available."));
    return false;
  }
}

void MicrophoneManager::OpenInternal() {
  DCHECK(thread_.message_loop_proxy()->BelongsToCurrentThread());

  // Try to create a valid microphone if necessary.
  if (state_ == kStarted || !CreateIfNecessary()) {
    return;
  }

  DCHECK(microphone_);
  if (!microphone_->Open()) {
    state_ = kError;
    error_callback_.Run(new SpeechRecognitionError(
        SpeechRecognitionError::kAborted, "Microphone open failed."));
    return;
  }

  poll_mic_events_timer_.emplace();
  // Setup a timer to poll for input events.
  poll_mic_events_timer_->Start(
      FROM_HERE, base::TimeDelta::FromMicroseconds(static_cast<int64>(
                     base::Time::kMicrosecondsPerSecond / kMicReadRateInHertz)),
      this, &MicrophoneManager::Read);
  state_ = kStarted;
}

void MicrophoneManager::CloseInternal() {
  DCHECK(thread_.message_loop_proxy()->BelongsToCurrentThread());

  if (state_ == kStopped) {
    return;
  }

  if (poll_mic_events_timer_) {
    poll_mic_events_timer_->Stop();
  }

  if (microphone_) {
    if (!microphone_->Close()) {
      state_ = kError;
      error_callback_.Run(new SpeechRecognitionError(
          SpeechRecognitionError::kAborted, "Microphone close failed."));
      return;
    }
    completion_callback_.Run();
    state_ = kStopped;
  }
}

void MicrophoneManager::Read() {
  DCHECK(thread_.message_loop_proxy()->BelongsToCurrentThread());

  DCHECK(state_ == kStarted);
  DCHECK(microphone_);
  DCHECK(microphone_->MinMicrophoneReadInBytes() <= kBufferSizeInBytes);

  static int16_t samples[kBufferSizeInBytes / sizeof(int16_t)];
  int read_bytes =
      microphone_->Read(reinterpret_cast<char*>(samples), kBufferSizeInBytes);
  if (read_bytes > 0) {
    size_t frames = read_bytes / sizeof(int16_t);
    scoped_ptr<ShellAudioBus> output_audio_bus(new ShellAudioBus(
        1, frames, ShellAudioBus::kInt16, ShellAudioBus::kInterleaved));
    output_audio_bus->Assign(ShellAudioBus(1, frames, samples));
    data_received_callback_.Run(output_audio_bus.Pass());
  } else if (read_bytes < 0) {
    state_ = kError;
    error_callback_.Run(new SpeechRecognitionError(
        SpeechRecognitionError::kAborted, "Microphone read failed."));
    poll_mic_events_timer_->Stop();
  }
}

void MicrophoneManager::DestroyInternal() {
  DCHECK(thread_.message_loop_proxy()->BelongsToCurrentThread());

  microphone_.reset();
  state_ = kStopped;
  poll_mic_events_timer_ = base::nullopt;
}

}  // namespace speech
}  // namespace cobalt
