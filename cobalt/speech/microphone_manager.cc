// Copyright 2016 The Cobalt Authors. All Rights Reserved.
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

#include "cobalt/speech/microphone_manager.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/check.h"
#include "base/message_loop/message_pump_type.h"
#include "cobalt/base/task_runner_util.h"

namespace cobalt {
namespace speech {

namespace {
// Size of an audio buffer.
const int kBufferSizeInBytes = 8 * 1024;
// The frequency which we read the data from devices.
const float kMicReadRateInHertz = 60.0f;
}  // namespace

MicrophoneManager::MicrophoneManager(
    const DataReceivedCallback& data_received,
    const SuccessfulOpenCallback& successful_open,
    const CompletionCallback& completion, const ErrorCallback& error,
    const MicrophoneCreator& microphone_creator)
    : data_received_callback_(data_received),
      completion_callback_(completion),
      error_callback_(error),
      successful_open_callback_(successful_open),
      microphone_creator_(microphone_creator),
      state_(kStopped),
      thread_("MicrophoneThd") {
  thread_.StartWithOptions(base::Thread::Options(base::MessagePumpType::IO, 0));
}

MicrophoneManager::~MicrophoneManager() {
  base::task_runner_util::PostBlockingTask(
      thread_.task_runner(), FROM_HERE,
      base::BindOnce(&MicrophoneManager::DestroyInternal,
                     base::Unretained(this)));
}

void MicrophoneManager::Open() {
  thread_.task_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(&MicrophoneManager::OpenInternal, base::Unretained(this)));
}

void MicrophoneManager::Close() {
  base::task_runner_util::PostBlockingTask(
      thread_.task_runner(), FROM_HERE,
      base::BindOnce(&MicrophoneManager::CloseInternal,
                     base::Unretained(this)));
}

bool MicrophoneManager::CreateIfNecessary() {
  DCHECK(thread_.task_runner()->RunsTasksInCurrentSequence());

  if (microphone_) {
    return true;
  }

  microphone_ = microphone_creator_.Run(kBufferSizeInBytes);
  if (microphone_ && microphone_->IsValid()) {
    state_ = kStopped;
    return true;
  } else {
    DLOG(WARNING) << "Microphone creation failed.";
    microphone_.reset();
    state_ = kError;
    error_callback_.Run(MicrophoneError::kAudioCapture,
                        "No microphone available.");
    return false;
  }
}

void MicrophoneManager::OpenInternal() {
  DCHECK(thread_.task_runner()->RunsTasksInCurrentSequence());

  // Try to create a valid microphone if necessary.
  if (state_ == kStarted || !CreateIfNecessary()) {
    return;
  }

  DCHECK(microphone_);
  if (!microphone_->Open()) {
    state_ = kError;
    error_callback_.Run(MicrophoneError::kAborted, "Microphone open failed.");
    return;
  }

  if (!successful_open_callback_.is_null()) {
    successful_open_callback_.Run();
  }
  poll_mic_events_timer_.emplace();
  // Setup a timer to poll for input events.
  poll_mic_events_timer_->Start(
      FROM_HERE,
      base::TimeDelta::FromMicroseconds(static_cast<int64>(
          base::Time::kMicrosecondsPerSecond / kMicReadRateInHertz)),
      this, &MicrophoneManager::Read);
  state_ = kStarted;
}

void MicrophoneManager::CloseInternal() {
  DCHECK(thread_.task_runner()->RunsTasksInCurrentSequence());

  if (state_ == kStopped) {
    return;
  }

  if (poll_mic_events_timer_) {
    poll_mic_events_timer_->Stop();
  }

  if (microphone_) {
    if (!microphone_->Close()) {
      state_ = kError;
      error_callback_.Run(MicrophoneError::kAborted,
                          "Microphone close failed.");
      return;
    }
    completion_callback_.Run();
    state_ = kStopped;
  }
}

void MicrophoneManager::Read() {
  DCHECK(thread_.task_runner()->RunsTasksInCurrentSequence());

  DCHECK(state_ == kStarted);
  DCHECK(microphone_);
  DCHECK(microphone_->MinMicrophoneReadInBytes() <= kBufferSizeInBytes);

  int16_t samples[kBufferSizeInBytes / sizeof(int16_t)];
  int read_bytes =
      microphone_->Read(reinterpret_cast<char*>(samples), kBufferSizeInBytes);
  // If |read_bytes| is zero, nothing should happen.
  if (read_bytes > 0 && read_bytes % sizeof(int16_t) == 0) {
    size_t frames = read_bytes / sizeof(int16_t);
    std::unique_ptr<AudioBus> output_audio_bus(
        new AudioBus(1, frames, AudioBus::kInt16, AudioBus::kInterleaved));
    AudioBus source(1, frames, samples);
    output_audio_bus->Assign(source);
    data_received_callback_.Run(std::move(output_audio_bus));
  } else if (read_bytes != 0) {
    state_ = kError;
    error_callback_.Run(MicrophoneError::kAborted, "Microphone read failed.");
    poll_mic_events_timer_->Stop();
  }
}

void MicrophoneManager::DestroyInternal() {
  DCHECK(thread_.task_runner()->RunsTasksInCurrentSequence());

  microphone_.reset();
  state_ = kStopped;
  poll_mic_events_timer_ = base::nullopt;
}

}  // namespace speech
}  // namespace cobalt
