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
}  // namespace

SpeechRecognitionManager::SpeechRecognitionManager(
    loader::FetcherFactory* fetcher_factory)
    : ALLOW_THIS_IN_INITIALIZER_LIST(weak_ptr_factory_(this)),
      weak_this_(weak_ptr_factory_.GetWeakPtr()),
      main_message_loop_(base::MessageLoopProxy::current()),
      recognizer_(fetcher_factory),
      ALLOW_THIS_IN_INITIALIZER_LIST(mic_(Mic::Create(
          kSampleRate, base::Bind(&SpeechRecognitionManager::OnDataReceived,
                                  base::Unretained(this)),
          base::Bind(&SpeechRecognitionManager::OnDataCompletion,
                     base::Unretained(this)),
          base::Bind(&SpeechRecognitionManager::OnError,
                     base::Unretained(this))))) {}

SpeechRecognitionManager::~SpeechRecognitionManager() { Stop(); }

void SpeechRecognitionManager::Start(const SpeechRecognitionConfig& config) {
  DCHECK(main_message_loop_->BelongsToCurrentThread());

  recognizer_.Start(config);
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

  // TODO: Encode audio data, and then send it to recognizer. After
  // receiving the recognition result from recognizer, fire a speech recognition
  // event.
}

void SpeechRecognitionManager::OnDataCompletion() {
  if (!main_message_loop_->BelongsToCurrentThread()) {
    // Called from mic thread.
    main_message_loop_->PostTask(
        FROM_HERE,
        base::Bind(&SpeechRecognitionManager::OnDataCompletion, weak_this_));
    return;
  }

  // TODO: Handle the case that no audio data would be received
  // afterwards.
}

void SpeechRecognitionManager::OnError() {
  if (!main_message_loop_->BelongsToCurrentThread()) {
    // Called from mic thread.
    main_message_loop_->PostTask(
        FROM_HERE, base::Bind(&SpeechRecognitionManager::OnError, weak_this_));
    return;
  }

  // TODO: Handle the case that an error occurred.
}

}  // namespace speech
}  // namespace cobalt
