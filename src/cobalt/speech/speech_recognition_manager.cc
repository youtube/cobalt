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

#include "cobalt/speech/speech_recognition_manager.h"

#include "base/bind.h"
#include "cobalt/dom/dom_exception.h"

namespace cobalt {
namespace speech {

namespace {
const int kSampleRate = 16000;
const float kAudioPacketDurationInSeconds = 0.1f;
}  // namespace

SpeechRecognitionManager::SpeechRecognitionManager(
    network::NetworkModule* network_module, const EventCallback& event_callback)
    : ALLOW_THIS_IN_INITIALIZER_LIST(weak_ptr_factory_(this)),
      weak_this_(weak_ptr_factory_.GetWeakPtr()),
      main_message_loop_(base::MessageLoopProxy::current()),
      event_callback_(event_callback),
      ALLOW_THIS_IN_INITIALIZER_LIST(
          recognizer_(network_module,
                      base::Bind(&SpeechRecognitionManager::OnRecognizerEvent,
                                 base::Unretained(this)))),
      ALLOW_THIS_IN_INITIALIZER_LIST(mic_(Mic::Create(
          kSampleRate, base::Bind(&SpeechRecognitionManager::OnDataReceived,
                                  base::Unretained(this)),
          base::Bind(&SpeechRecognitionManager::OnDataCompletion,
                     base::Unretained(this)),
          base::Bind(&SpeechRecognitionManager::OnMicError,
                     base::Unretained(this))))),
      state_(kStopped) {}

SpeechRecognitionManager::~SpeechRecognitionManager() { Stop(); }

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

  recognizer_.Start(config, kSampleRate);
  mic_->Start();
  state_ = kStarted;
}

void SpeechRecognitionManager::Stop() {
  DCHECK(main_message_loop_->BelongsToCurrentThread());

  // If the stop method is called on an object which is already stopped or being
  // stopped, the user agent MUST ignore the call.
  if (state_ != kStarted) {
    return;
  }

  mic_->Stop();
  recognizer_.Stop();
  state_ = kStopped;
}

void SpeechRecognitionManager::Abort() {
  DCHECK(main_message_loop_->BelongsToCurrentThread());

  // If the abort method is called on an object which is already stopped or
  // aborting, the user agent MUST ignore the call.
  if (state_ != kStarted) {
    return;
  }

  mic_->Stop();
  recognizer_.Stop();
  state_ = kAborted;
}

void SpeechRecognitionManager::OnDataReceived(
    scoped_ptr<ShellAudioBus> audio_bus) {
  if (!main_message_loop_->BelongsToCurrentThread()) {
    // Called from mic thread.
    main_message_loop_->PostTask(
        FROM_HERE, base::Bind(&SpeechRecognitionManager::OnDataReceived,
                              weak_this_, base::Passed(&audio_bus)));
    return;
  }

  // Stop recognizing if in the abort state.
  if (state_ != kAborted) {
    recognizer_.RecognizeAudio(audio_bus.Pass(), false);
  }
}

void SpeechRecognitionManager::OnDataCompletion() {
  if (!main_message_loop_->BelongsToCurrentThread()) {
    // Called from mic thread.
    main_message_loop_->PostTask(
        FROM_HERE,
        base::Bind(&SpeechRecognitionManager::OnDataCompletion, weak_this_));
    return;
  }

  // Stop recognizing if in the abort state.
  if (state_ != kAborted) {
    // The encoder requires a non-empty final buffer, so encoding a packet of
    // silence at the end in case encoder had no data already.
    size_t dummy_frames =
        static_cast<size_t>(kSampleRate * kAudioPacketDurationInSeconds);
    scoped_ptr<ShellAudioBus> dummy_audio_bus(new ShellAudioBus(
        1, dummy_frames, ShellAudioBus::kInt16, ShellAudioBus::kPlanar));
    dummy_audio_bus->ZeroAllFrames();
    recognizer_.RecognizeAudio(dummy_audio_bus.Pass(), true);
  }
}

void SpeechRecognitionManager::OnRecognizerEvent(
    const scoped_refptr<dom::Event>& event) {
  if (!main_message_loop_->BelongsToCurrentThread()) {
    // Called from recognizer thread.
    main_message_loop_->PostTask(
        FROM_HERE, base::Bind(&SpeechRecognitionManager::OnRecognizerEvent,
                              weak_this_, event));
    return;
  }

  // Do not return any information if in the abort state.
  if (state_ != kAborted) {
    event_callback_.Run(event);
  }
}

void SpeechRecognitionManager::OnMicError() {
  if (!main_message_loop_->BelongsToCurrentThread()) {
    // Called from mic thread.
    main_message_loop_->PostTask(
        FROM_HERE,
        base::Bind(&SpeechRecognitionManager::OnMicError, weak_this_));
    return;
  }

  event_callback_.Run(
      scoped_refptr<SpeechRecognitionError>(new SpeechRecognitionError(
          SpeechRecognitionError::kAborted, "Mic Disconnected.")));

  // An error is occured in Mic, so stopping the recognizer.
  recognizer_.Stop();
  state_ = kAborted;
}

}  // namespace speech
}  // namespace cobalt
