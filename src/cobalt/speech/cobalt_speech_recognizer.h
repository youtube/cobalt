// Copyright 2017 The Cobalt Authors. All Rights Reserved.
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

#ifndef COBALT_SPEECH_COBALT_SPEECH_RECOGNIZER_H_
#define COBALT_SPEECH_COBALT_SPEECH_RECOGNIZER_H_

#include <memory>
#include <string>

#include "cobalt/media/base/audio_bus.h"
#include "cobalt/network/network_module.h"
#include "cobalt/speech/endpointer_delegate.h"
#include "cobalt/speech/google_speech_service.h"
#include "cobalt/speech/microphone.h"
#include "cobalt/speech/microphone_manager.h"
#include "cobalt/speech/speech_configuration.h"
#include "cobalt/speech/speech_recognition_config.h"
#include "cobalt/speech/speech_recognition_error.h"
#include "cobalt/speech/speech_recognition_event.h"
#include "cobalt/speech/speech_recognizer.h"

namespace cobalt {
namespace speech {

// Cobalt's implementation of speech recognizer which interacts with Google
// speech service and device's microphone. It collects the microphone PCM audio
// data and sends it to Google speech service, then gets the recognition results
// from there.
class CobaltSpeechRecognizer : public SpeechRecognizer {
 public:
  typedef media::AudioBus AudioBus;

  CobaltSpeechRecognizer(network::NetworkModule* network_module,
                         const Microphone::Options& microphone_options,
                         const EventCallback& event_callback);
  ~CobaltSpeechRecognizer();

  void Start(const SpeechRecognitionConfig& config) override;
  void Stop() override;

 private:
  // Callbacks from mic.
  void OnDataReceived(std::unique_ptr<AudioBus> audio_bus);
  void OnDataCompletion();
  void OnMicrophoneError(MicrophoneManager::MicrophoneError error,
                         std::string error_message);

  // Callbacks from recognizer.
  void OnRecognizerEvent(const scoped_refptr<dom::Event>& event);

  EventCallback event_callback_;
  std::unique_ptr<GoogleSpeechService> service_;
  std::unique_ptr<MicrophoneManager> microphone_manager_;
  // Delegate of endpointer which is used for detecting sound energy.
  EndPointerDelegate endpointer_delegate_;
};

}  // namespace speech
}  // namespace cobalt

#endif  // COBALT_SPEECH_COBALT_SPEECH_RECOGNIZER_H_
