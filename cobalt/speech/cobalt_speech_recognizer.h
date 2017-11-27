// Copyright 2017 Google Inc. All Rights Reserved.
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
#if defined(COBALT_MEDIA_SOURCE_2016)
#include "cobalt/media/base/shell_audio_bus.h"
#else  // defined(COBALT_MEDIA_SOURCE_2016)
#include "media/base/shell_audio_bus.h"
#endif  // defined(COBALT_MEDIA_SOURCE_2016)

namespace cobalt {
namespace speech {

// Cobalt's implementation of speech recognizer which interacts with Google
// speech service and device's microphone. It collects the microphone PCM audio
// data and sends it to Google speech service, then gets the recognition results
// from there.
class CobaltSpeechRecognizer : public SpeechRecognizer {
 public:
#if defined(COBALT_MEDIA_SOURCE_2016)
  typedef media::ShellAudioBus ShellAudioBus;
#else   // defined(COBALT_MEDIA_SOURCE_2016)
  typedef ::media::ShellAudioBus ShellAudioBus;
#endif  // defined(COBALT_MEDIA_SOURCE_2016)

  CobaltSpeechRecognizer(network::NetworkModule* network_module,
                         const Microphone::Options& microphone_options,
                         const EventCallback& event_callback);
  ~CobaltSpeechRecognizer();

  void Start(const SpeechRecognitionConfig& config) override;
  void Stop() override;

 private:
  // Callbacks from mic.
  void OnDataReceived(scoped_ptr<ShellAudioBus> audio_bus);
  void OnDataCompletion();
  void OnMicError(const scoped_refptr<dom::Event>& event);

  // Callbacks from recognizer.
  void OnRecognizerEvent(const scoped_refptr<dom::Event>& event);

  EventCallback event_callback_;
  scoped_ptr<GoogleSpeechService> service_;
  scoped_ptr<MicrophoneManager> microphone_manager_;
  // Delegate of endpointer which is used for detecting sound energy.
  EndPointerDelegate endpointer_delegate_;
};

}  // namespace speech
}  // namespace cobalt

#endif  // COBALT_SPEECH_COBALT_SPEECH_RECOGNIZER_H_
