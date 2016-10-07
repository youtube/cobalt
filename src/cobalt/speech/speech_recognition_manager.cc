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
                      base::Bind(&SpeechRecognitionManager::OnRecognizerResult,
                                 base::Unretained(this)),
                      base::Bind(&SpeechRecognitionManager::OnRecognizerError,
                                 base::Unretained(this)))),
      ALLOW_THIS_IN_INITIALIZER_LIST(mic_(Mic::Create(
          kSampleRate, base::Bind(&SpeechRecognitionManager::OnDataReceived,
                                  base::Unretained(this)),
          base::Bind(&SpeechRecognitionManager::OnDataCompletion,
                     base::Unretained(this)),
          base::Bind(&SpeechRecognitionManager::OnMicError,
                     base::Unretained(this))))) {}

SpeechRecognitionManager::~SpeechRecognitionManager() { Stop(); }

void SpeechRecognitionManager::Start(const SpeechRecognitionConfig& config) {
  DCHECK(main_message_loop_->BelongsToCurrentThread());

  recognizer_.Start(config, kSampleRate);
  mic_->Start();
}

void SpeechRecognitionManager::Stop() {
  DCHECK(main_message_loop_->BelongsToCurrentThread());

  mic_->Stop();
  recognizer_.Stop();
}

void SpeechRecognitionManager::OnDataReceived(scoped_ptr<AudioBus> audio_bus) {
  if (!main_message_loop_->BelongsToCurrentThread()) {
    // Called from mic thread.
    main_message_loop_->PostTask(
        FROM_HERE, base::Bind(&SpeechRecognitionManager::OnDataReceived,
                              weak_this_, base::Passed(&audio_bus)));
    return;
  }

  recognizer_.RecognizeAudio(audio_bus.Pass(), false);
}

void SpeechRecognitionManager::OnDataCompletion() {
  if (!main_message_loop_->BelongsToCurrentThread()) {
    // Called from mic thread.
    main_message_loop_->PostTask(
        FROM_HERE,
        base::Bind(&SpeechRecognitionManager::OnDataCompletion, weak_this_));
    return;
  }

  // The encoder requires a non-empty final buffer, so encoding a packet of
  // silence at the end in case encoder had no data already.
  size_t dummy_frames =
      static_cast<size_t>(kSampleRate * kAudioPacketDurationInSeconds);
  scoped_ptr<AudioBus> dummy_audio_bus =
      AudioBus::Create(1, static_cast<int>(dummy_frames));
  memset(dummy_audio_bus->channel(0), 0, dummy_frames);
  recognizer_.RecognizeAudio(dummy_audio_bus.Pass(), true);
}

void SpeechRecognitionManager::OnRecognizerResult(
    const scoped_refptr<SpeechRecognitionEvent>& event) {
  if (!main_message_loop_->BelongsToCurrentThread()) {
    // Called from recognizer thread.
    main_message_loop_->PostTask(
        FROM_HERE, base::Bind(&SpeechRecognitionManager::OnRecognizerResult,
                              weak_this_, event));
    return;
  }

  event_callback_.Run(event);
}

void SpeechRecognitionManager::OnRecognizerError() {
  if (!main_message_loop_->BelongsToCurrentThread()) {
    // Called from recognizer thread.
    main_message_loop_->PostTask(
        FROM_HERE,
        base::Bind(&SpeechRecognitionManager::OnRecognizerError, weak_this_));
    return;
  }

  // TODO: Could be other error types based on the recognizer response.
  event_callback_.Run(
      scoped_refptr<SpeechRecognitionError>(new SpeechRecognitionError(
          SpeechRecognitionError::kNetwork, "Recognition Failed.")));
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
}

}  // namespace speech
}  // namespace cobalt
