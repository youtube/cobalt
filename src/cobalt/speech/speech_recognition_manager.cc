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

#include "cobalt/speech/speech_recognition_manager.h"

#include "base/bind.h"
#include "cobalt/dom/dom_exception.h"
#include "cobalt/speech/speech_configuration.h"
#include "cobalt/speech/speech_recognition_error.h"
#if defined(SB_USE_SB_SPEECH_RECOGNIZER)
#include "cobalt/speech/starboard_speech_recognizer.h"
#endif
#include "cobalt/speech/cobalt_speech_recognizer.h"

namespace cobalt {
namespace speech {

SpeechRecognitionManager::SpeechRecognitionManager(
    network::NetworkModule* network_module, const EventCallback& event_callback,
    const Microphone::Options& microphone_options)
    : ALLOW_THIS_IN_INITIALIZER_LIST(weak_ptr_factory_(this)),
      weak_this_(weak_ptr_factory_.GetWeakPtr()),
      main_message_loop_task_runner_(
          base::MessageLoop::current()->task_runner()),
      event_callback_(event_callback),
      state_(kStopped) {
#if defined(SB_USE_SB_SPEECH_RECOGNIZER)
  if (StarboardSpeechRecognizer::IsSupported()) {
    recognizer_.reset(new StarboardSpeechRecognizer(base::Bind(
        &SpeechRecognitionManager::OnEventAvailable, base::Unretained(this))));
    return;
  }
#endif  // defined(SB_USE_SB_SPEECH_RECOGNIZER)
  if (GoogleSpeechService::GetSpeechAPIKey()) {
    recognizer_.reset(new CobaltSpeechRecognizer(
        network_module, microphone_options,
        base::Bind(&SpeechRecognitionManager::OnEventAvailable,
                   base::Unretained(this))));
  }
}

SpeechRecognitionManager::~SpeechRecognitionManager() { Abort(); }

void SpeechRecognitionManager::Start(const SpeechRecognitionConfig& config,
                                     script::ExceptionState* exception_state) {
  DCHECK(main_message_loop_task_runner_->BelongsToCurrentThread());

  // If the start method is called on an already started object, the user agent
  // MUST throw an InvalidStateError exception and ignore the call.
  if (state_ == kStarted) {
    dom::DOMException::Raise(dom::DOMException::kInvalidStateErr,
                             exception_state);
    return;
  }

  // If no recognizer is available on this platform, immediately generate a
  // "no-speech" error.
  //   https://w3c.github.io/speech-api/speechapi.html#speechreco-events
  if (!recognizer_) {
    OnEventAvailable(
        new SpeechRecognitionError(kSpeechRecognitionErrorCodeNoSpeech, ""));
    return;
  }

  recognizer_->Start(config);
  state_ = kStarted;
}

void SpeechRecognitionManager::Stop() {
  DCHECK(main_message_loop_task_runner_->BelongsToCurrentThread());

  // If the stop method is called on an object which is already stopped or being
  // stopped, the user agent MUST ignore the call.
  if (state_ != kStarted) {
    return;
  }

  recognizer_->Stop();
  state_ = kStopped;
}

void SpeechRecognitionManager::Abort() {
  DCHECK(main_message_loop_task_runner_->BelongsToCurrentThread());

  // If the abort method is called on an object which is already stopped or
  // aborting, the user agent MUST ignore the call.
  if (state_ != kStarted) {
    return;
  }

  state_ = kAborted;
  recognizer_->Stop();
}

void SpeechRecognitionManager::OnEventAvailable(
    const scoped_refptr<dom::Event>& event) {
  if (!main_message_loop_task_runner_->BelongsToCurrentThread()) {
    // Called from recognizer. |event_callback_| is required to be run on
    // the |main_message_loop_task_runner_|.
    main_message_loop_task_runner_->PostTask(
        FROM_HERE, base::Bind(&SpeechRecognitionManager::OnEventAvailable,
                              weak_this_, event));
    return;
  }

  // Do not return any information if in the abort state.
  if (state_ != kAborted) {
    event_callback_.Run(event);
  }
}

}  // namespace speech
}  // namespace cobalt
