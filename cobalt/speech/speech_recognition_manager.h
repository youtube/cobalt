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

#ifndef COBALT_SPEECH_SPEECH_RECOGNITION_MANAGER_H_
#define COBALT_SPEECH_SPEECH_RECOGNITION_MANAGER_H_

#include <memory>
#include <string>

#include "cobalt/network/network_module.h"
#include "cobalt/script/exception_state.h"
#include "cobalt/speech/microphone.h"
#include "cobalt/speech/speech_recognition_config.h"
#include "cobalt/speech/speech_recognizer.h"

namespace cobalt {
namespace speech {

// Owned by SpeechRecognition to manage major speech recognition logic.
// This class interacts with microphone, speech recognition service and audio
// encoder. It provides the interface to start/stop microphone and
// recognition service. After receiving the audio data from microphone, this
// class would encode the audio data, then send it to recognition service.
class SpeechRecognitionManager {
 public:
  typedef base::Callback<bool(const scoped_refptr<dom::Event>&)> EventCallback;

  SpeechRecognitionManager(network::NetworkModule* network_module,
                           const EventCallback& event_callback,
                           const Microphone::Options& microphone_options);
  ~SpeechRecognitionManager();

  // Start/Stop speech recognizer.
  void Start(const SpeechRecognitionConfig& config,
             script::ExceptionState* exception_state);
  void Stop();
  void Abort();

 private:
  enum State {
    kStarted,
    kStopped,
    kAborted,
  };

  void OnEventAvailable(const scoped_refptr<dom::Event>& event);

  base::WeakPtrFactory<SpeechRecognitionManager> weak_ptr_factory_;
  // We construct a WeakPtr upon SpeechRecognitionManager's construction in
  // order to associate the WeakPtr with the constructing thread.
  base::WeakPtr<SpeechRecognitionManager> weak_this_;
  scoped_refptr<base::SingleThreadTaskRunner> const
      main_message_loop_task_runner_;

  // Callback for sending dom events if available.
  EventCallback event_callback_;
  std::unique_ptr<SpeechRecognizer> recognizer_;

  State state_;
};

}  // namespace speech
}  // namespace cobalt

#endif  // COBALT_SPEECH_SPEECH_RECOGNITION_MANAGER_H_
