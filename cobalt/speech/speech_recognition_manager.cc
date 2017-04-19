// Copyright 2016 Google Inc. All Rights Reserved.
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

namespace cobalt {
namespace speech {

SpeechRecognitionManager::SpeechRecognitionManager(
    network::NetworkModule* network_module, const EventCallback& event_callback,
    const Microphone::Options& microphone_options)
    : ALLOW_THIS_IN_INITIALIZER_LIST(weak_ptr_factory_(this)),
      weak_this_(weak_ptr_factory_.GetWeakPtr()),
      main_message_loop_(base::MessageLoopProxy::current()),
      event_callback_(event_callback),
      recognizer_(new CobaltSpeechRecognizer(
          network_module, microphone_options,
          base::Bind(&SpeechRecognitionManager::OnSuccess,
                     base::Unretained(this)),
          base::Bind(&SpeechRecognitionManager::OnError,
                     base::Unretained(this)))),
      state_(kStopped) {}

SpeechRecognitionManager::~SpeechRecognitionManager() { Abort(); }

void SpeechRecognitionManager::Start(const SpeechRecognitionConfig& config,
                                     script::ExceptionState* exception_state) {
  DCHECK(main_message_loop_->BelongsToCurrentThread());

  // If the start method is called on an already started object, the user agent
  // MUST throw an InvalidStateError exception and ignore the call.
  if (state_ == kStarted) {
    dom::DOMException::Raise(dom::DOMException::kInvalidStateErr,
                             exception_state);
    return;
  }

  recognizer_->Start(config);
  state_ = kStarted;
}

void SpeechRecognitionManager::Stop() {
  DCHECK(main_message_loop_->BelongsToCurrentThread());

  // If the stop method is called on an object which is already stopped or being
  // stopped, the user agent MUST ignore the call.
  if (state_ != kStarted) {
    return;
  }

  recognizer_->Stop();
  state_ = kStopped;
}

void SpeechRecognitionManager::Abort() {
  DCHECK(main_message_loop_->BelongsToCurrentThread());

  // If the abort method is called on an object which is already stopped or
  // aborting, the user agent MUST ignore the call.
  if (state_ != kStarted) {
    return;
  }

  state_ = kAborted;
  recognizer_->Stop();
}

void SpeechRecognitionManager::OnSuccess(
    const scoped_refptr<dom::Event>& event) {
  if (!main_message_loop_->BelongsToCurrentThread()) {
    // Called from recognizer. |event_callback_| is required to be run on
    // the |main_message_loop_|.
    main_message_loop_->PostTask(
        FROM_HERE,
        base::Bind(&SpeechRecognitionManager::OnSuccess, weak_this_, event));
    return;
  }

  // Do not return any information if in the abort state.
  if (state_ != kAborted) {
    event_callback_.Run(event);
  }
}

void SpeechRecognitionManager::OnError(const scoped_refptr<dom::Event>& event) {
  if (!main_message_loop_->BelongsToCurrentThread()) {
    // Called from recognizer. |event_callback_| is required to be run on
    // the |main_message_loop_|.
    main_message_loop_->PostTask(
        FROM_HERE,
        base::Bind(&SpeechRecognitionManager::OnError, weak_this_, event));
    return;
  }

  if (state_ != kAborted) {
    event_callback_.Run(event);
  }
}

}  // namespace speech
}  // namespace cobalt
