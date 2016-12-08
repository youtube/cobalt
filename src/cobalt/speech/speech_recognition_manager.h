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

#ifndef COBALT_SPEECH_SPEECH_RECOGNITION_MANAGER_H_
#define COBALT_SPEECH_SPEECH_RECOGNITION_MANAGER_H_

#include <string>

#include "cobalt/loader/fetcher_factory.h"
#include "cobalt/speech/mic.h"
#include "cobalt/speech/speech_recognition_config.h"
#include "cobalt/speech/speech_recognizer.h"

namespace cobalt {
namespace speech {

// Owned by SpeechRecognition to manage major speech recognition logic.
// This class interacts with microphone, speech recognition service and audio
// encoder. It provides the interface to start/stop microphone and
// recogniton service. After receiving the audio data from microphone, this
// class would encode the audio data, then send it to recogniton service.
class SpeechRecognitionManager {
 public:
  typedef ::media::AudioBus AudioBus;

  explicit SpeechRecognitionManager(loader::FetcherFactory* fetcher_factory);
  ~SpeechRecognitionManager();

  // Start/Stop speech recognizer and microphone. Multiple calls would be
  // managed by their own class.
  void Start(const SpeechRecognitionConfig& config);
  void Stop();

 private:
  // Callbacks from mic.
  void OnDataReceived(scoped_ptr<AudioBus> audio_bus);
  void OnDataCompletion();
  void OnError();

  base::WeakPtrFactory<SpeechRecognitionManager> weak_ptr_factory_;
  // We construct a WeakPtr upon SpeechRecognitionManager's construction in
  // order to associate the WeakPtr with the constructing thread.
  base::WeakPtr<SpeechRecognitionManager> weak_this_;

  scoped_refptr<base::MessageLoopProxy> const main_message_loop_;
  SpeechRecognizer recognizer_;
  scoped_ptr<Mic> mic_;
};

}  // namespace speech
}  // namespace cobalt

#endif  // COBALT_SPEECH_SPEECH_RECOGNITION_MANAGER_H_
