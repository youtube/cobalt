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

#ifndef COBALT_SPEECH_SPEECH_RECOGNITION_MANAGER_H_
#define COBALT_SPEECH_SPEECH_RECOGNITION_MANAGER_H_

#include <string>

#include "cobalt/network/network_module.h"
#include "cobalt/script/exception_state.h"
#include "cobalt/speech/endpointer_delegate.h"
#include "cobalt/speech/microphone.h"
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

class MicrophoneManager;

// Owned by SpeechRecognition to manage major speech recognition logic.
// This class interacts with microphone, speech recognition service and audio
// encoder. It provides the interface to start/stop microphone and
// recogniton service. After receiving the audio data from microphone, this
// class would encode the audio data, then send it to recogniton service.
class SpeechRecognitionManager {
 public:
#if defined(COBALT_MEDIA_SOURCE_2016)
  typedef media::ShellAudioBus ShellAudioBus;
#else   // defined(COBALT_MEDIA_SOURCE_2016)
  typedef ::media::ShellAudioBus ShellAudioBus;
#endif  // defined(COBALT_MEDIA_SOURCE_2016)
  typedef base::Callback<bool(const scoped_refptr<dom::Event>&)> EventCallback;

  SpeechRecognitionManager(network::NetworkModule* network_module,
                           const EventCallback& event_callback,
                           const Microphone::Options& microphone_options);
  ~SpeechRecognitionManager();

  // Start/Stop speech recognizer and microphone. Multiple calls would be
  // managed by their own class.
  void Start(const SpeechRecognitionConfig& config,
             script::ExceptionState* exception_state);
  void Stop(bool run_callback = true);
  void Abort();

 private:
  enum State {
    kStarted,
    kStopped,
    kAborted,
  };

  // Callbacks from mic.
  void OnDataReceived(scoped_ptr<ShellAudioBus> audio_bus);
  void OnDataCompletion();
  void OnMicError(const scoped_refptr<dom::Event>& event);

  // Callbacks from recognizer.
  void OnRecognizerEvent(const scoped_refptr<dom::Event>& event);

  base::WeakPtrFactory<SpeechRecognitionManager> weak_ptr_factory_;
  // We construct a WeakPtr upon SpeechRecognitionManager's construction in
  // order to associate the WeakPtr with the constructing thread.
  base::WeakPtr<SpeechRecognitionManager> weak_this_;
  scoped_refptr<base::MessageLoopProxy> const main_message_loop_;

  // Callback for sending dom events if available.
  EventCallback event_callback_;
  scoped_ptr<SpeechRecognizer> recognizer_;

  scoped_ptr<MicrophoneManager> microphone_manager_;

  // Delegate of endpointer which is used for detecting sound energy.
  EndPointerDelegate endpointer_delegate_;

  State state_;
};

}  // namespace speech
}  // namespace cobalt

#endif  // COBALT_SPEECH_SPEECH_RECOGNITION_MANAGER_H_
