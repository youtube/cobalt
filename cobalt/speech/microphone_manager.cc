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

#include "starboard/log.h"

#if defined(SB_USE_SB_MICROPHONE)

namespace cobalt {
namespace speech {

namespace {
// The maximum of microphones which can be supported. Currently only supports
// one microphone.
const int kNumberOfMicrophones = 1;
// Size of an audio buffer.
const int kBufferSizeInBytes = 8 * 1024;
// The frequency which we read the data from devices.
const float kMicReadRateInHertz = 60.0f;
}  // namespace

MicrophoneManager::MicrophoneManager(int sample_rate,
                                     const DataReceivedCallback& data_received,
                                     const CompletionCallback& completion,
                                     const ErrorCallback& error)
    : sample_rate_(sample_rate),
      data_received_callback_(data_received),
      completion_callback_(completion),
      error_callback_(error),
      microphone_(kSbMicrophoneInvalid),
      min_microphone_read_in_bytes_(-1),
      state_(kStopped),
      thread_("microphone_thread") {
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

void MicrophoneManager::CreateInternal() {
  SB_DCHECK(thread_.message_loop_proxy()->BelongsToCurrentThread());

  SbMicrophoneInfo info[kNumberOfMicrophones];
  int microphone_num = SbMicrophoneGetAvailable(info, kNumberOfMicrophones);

  SB_DCHECK(!SbMicrophoneIsValid(microphone_));
  // Loop all the available microphones and create a valid one.
  for (int index = 0; index < microphone_num; ++index) {
    if (!SbMicrophoneIsSampleRateSupported(info[index].id, sample_rate_)) {
      continue;
    }

    microphone_ =
        SbMicrophoneCreate(info[index].id, sample_rate_, kBufferSizeInBytes);
    if (!SbMicrophoneIsValid(microphone_)) {
      continue;
    }

    // Created a microphone successfully.
    min_microphone_read_in_bytes_ = info[index].min_read_size;
    state_ = kStopped;
    return;
  }

  SB_DLOG(WARNING) << "Microphone creation failed.";
  state_ = kError;
  error_callback_.Run();
}

void MicrophoneManager::OpenInternal() {
  SB_DCHECK(thread_.message_loop_proxy()->BelongsToCurrentThread());

  if (state_ == kStarted) {
    return;
  }

  // Try to create a valid microphone.
  if (!SbMicrophoneIsValid(microphone_)) {
    // If |microphone_| is invalid, try to create a microphone before doing
    // anything.
    CreateInternal();
    // If |microphone_| is still invalid, there is no need to go further.
    if (!SbMicrophoneIsValid(microphone_)) {
      return;
    }
  }

  SB_DCHECK(SbMicrophoneIsValid(microphone_));
  bool success = SbMicrophoneOpen(microphone_);
  if (!success) {
    state_ = kError;
    error_callback_.Run();
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
  SB_DCHECK(thread_.message_loop_proxy()->BelongsToCurrentThread());

  if (state_ == kStopped) {
    return;
  }

  poll_mic_events_timer_->Stop();

  SB_DCHECK(SbMicrophoneIsValid(microphone_));
  bool success = SbMicrophoneClose(microphone_);
  if (!success) {
    state_ = kError;
    error_callback_.Run();
    return;
  }

  completion_callback_.Run();
  state_ = kStopped;
}

void MicrophoneManager::Read() {
  DCHECK(thread_.message_loop_proxy()->BelongsToCurrentThread());

  SB_DCHECK(state_ == kStarted);
  SB_DCHECK(min_microphone_read_in_bytes_ <= kBufferSizeInBytes);

  static int16_t samples[kBufferSizeInBytes / sizeof(int16_t)];
  int read_bytes = SbMicrophoneRead(microphone_, samples, kBufferSizeInBytes);
  if (read_bytes > 0) {
    int frames = read_bytes / sizeof(int16_t);
    scoped_ptr<ShellAudioBus> output_audio_bus(new ShellAudioBus(
        1, frames, ShellAudioBus::kInt16, ShellAudioBus::kInterleaved));
    output_audio_bus->Assign(ShellAudioBus(1, frames, samples));
    data_received_callback_.Run(output_audio_bus.Pass());
  } else if (read_bytes < 0) {
    state_ = kError;
    error_callback_.Run();
    poll_mic_events_timer_->Stop();
  }
}

void MicrophoneManager::DestroyInternal() {
  SB_DCHECK(thread_.message_loop_proxy()->BelongsToCurrentThread());

  SbMicrophoneDestroy(microphone_);
  state_ = kStopped;
}

}  // namespace speech
}  // namespace cobalt

#endif  // defined(SB_USE_SB_MICROPHONE)
