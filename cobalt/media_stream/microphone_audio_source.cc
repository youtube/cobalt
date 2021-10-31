// Copyright 2018 The Cobalt Authors. All Rights Reserved.
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

#include "cobalt/media_stream/microphone_audio_source.h"

#include <memory>
#include <string>

#include "cobalt/media_stream/audio_parameters.h"
#include "cobalt/speech/microphone.h"
#include "cobalt/speech/microphone_fake.h"
#include "cobalt/speech/microphone_starboard.h"

#if SB_USE_SB_MICROPHONE && !defined(DISABLE_MICROPHONE_IDL)
#define ENABLE_MICROPHONE_IDL
#endif

namespace cobalt {
namespace media_stream {

bool MicrophoneAudioSource::EnsureSourceIsStarted() {
  microphone_manager_.Open();
  return true;
}

void MicrophoneAudioSource::EnsureSourceIsStopped() {
  microphone_manager_.Close();
  NotifyTracksOfNewReadyState(MediaStreamAudioTrack::kReadyStateEnded);
}

std::unique_ptr<cobalt::speech::Microphone>
MicrophoneAudioSource::CreateMicrophone(
    const cobalt::speech::Microphone::Options& options, int buffer_size_bytes) {
#if !defined(ENABLE_MICROPHONE_IDL)
  return std::unique_ptr<speech::Microphone>();
#else
  std::unique_ptr<speech::Microphone> mic;

#if defined(ENABLE_FAKE_MICROPHONE)
  if (options.enable_fake_microphone) {
    mic.reset(new speech::MicrophoneFake(options));
  }
#else
#endif  // defined(ENABLE_FAKE_MICROPHONE)

  if (!mic) {
    mic.reset(new speech::MicrophoneStarboard(
        speech::MicrophoneStarboard::kDefaultSampleRate, buffer_size_bytes));
  }

  if (mic) {
    AudioParameters params(
        1, speech::MicrophoneStarboard::kDefaultSampleRate,
        speech::MicrophoneStarboard::kSbMicrophoneSampleSizeInBytes * 8);
    SetFormat(params);
  }

  return mic;
#endif  // defined(ENABLE_MICROPHONE_IDL)
}

MicrophoneAudioSource::MicrophoneAudioSource(
    const speech::Microphone::Options& options,
    const MicrophoneAudioSource::SuccessfulOpenCallback& successful_open,
    const MicrophoneAudioSource::CompletionCallback& completion,
    const MicrophoneAudioSource::ErrorCallback& error)
    // Note: It is safe to use unretained pointers here, since
    // |microphone_manager_| is a member variable, and |this| object
    // will have the same lifetime.
    // Furthermore, it is an error to destruct the microphone manager
    // without stopping it, so these callbacks are not to be called
    // during the destruction of the object.
    : javascript_thread_task_runner_(
          base::MessageLoop::current()->task_runner()),
      ALLOW_THIS_IN_INITIALIZER_LIST(weak_ptr_factory_(this)),
      successful_open_callback_(successful_open),
      completion_callback_(completion),
      error_callback_(error),
      ALLOW_THIS_IN_INITIALIZER_LIST(microphone_manager_(
          base::Bind(&MicrophoneAudioSource::OnDataReceived,
                     base::Unretained(this)),
          base::Bind(&MicrophoneAudioSource::OnMicrophoneOpen,
                     base::Unretained(this)),
          base::Bind(&MicrophoneAudioSource::OnDataCompletion,
                     base::Unretained(this)),
          base::Bind(&MicrophoneAudioSource::OnMicrophoneError,
                     base::Unretained(this)),
          base::Bind(&MicrophoneAudioSource::CreateMicrophone,
                     base::Unretained(this), options))) {}

void MicrophoneAudioSource::OnDataReceived(
    std::unique_ptr<MediaStreamAudioTrack::AudioBus> audio_bus) {
  base::TimeTicks now = base::TimeTicks::Now();
  DeliverDataToTracks(*audio_bus, now);
}

void MicrophoneAudioSource::OnDataCompletion() {
  if (javascript_thread_task_runner_ !=
      base::MessageLoop::current()->task_runner()) {
    javascript_thread_task_runner_->PostTask(
        FROM_HERE, base::Bind(&MicrophoneAudioSource::OnDataCompletion,
                              weak_ptr_factory_.GetWeakPtr()));
    return;
  }
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DLOG(INFO) << "Microphone is closed.";
  StopSource();

  if (!completion_callback_.is_null()) {
    completion_callback_.Run();
  }
}

void MicrophoneAudioSource::OnMicrophoneOpen() {
  if (javascript_thread_task_runner_ !=
      base::MessageLoop::current()->task_runner()) {
    javascript_thread_task_runner_->PostTask(
        FROM_HERE, base::Bind(&MicrophoneAudioSource::OnMicrophoneOpen,
                              weak_ptr_factory_.GetWeakPtr()));
    return;
  }
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (!successful_open_callback_.is_null()) {
    successful_open_callback_.Run();
  }
}

void MicrophoneAudioSource::OnMicrophoneError(
    speech::MicrophoneManager::MicrophoneError error,
    std::string error_message) {
  if (javascript_thread_task_runner_ !=
      base::MessageLoop::current()->task_runner()) {
    javascript_thread_task_runner_->PostTask(
        FROM_HERE,
        base::Bind(&MicrophoneAudioSource::OnMicrophoneError,
                   weak_ptr_factory_.GetWeakPtr(), error, error_message));
    return;
  }
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  std::string microphone_error_category;
  switch (error) {
    case speech::MicrophoneManager::MicrophoneError::kAborted:
      // Unable to open the microphone.
      microphone_error_category = "Aborted";
      break;
    case speech::MicrophoneManager::MicrophoneError::kAudioCapture:
      microphone_error_category = "AudioCapture";
      break;
  }
  LOG(ERROR) << "Got a microphone error. Category[" << microphone_error_category
             << "] " << error_message;

  // StopSource() may result in the destruction of |this|, so we must ensure
  // that we do not reference members after this call.
  auto error_callback = error_callback_;

  // This will notify downstream objects audio track, and source that there will
  // be no more data.
  StopSource();

  if (!error_callback.is_null()) {
    error_callback.Run(error, error_message);
  }
}

}  // namespace media_stream
}  // namespace cobalt
