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

#ifndef COBALT_SPEECH_SPEECH_RECOGNIZER_H_
#define COBALT_SPEECH_SPEECH_RECOGNIZER_H_

#include "base/callback.h"
#include "cobalt/dom/event.h"
#include "cobalt/speech/speech_recognition_config.h"

namespace cobalt {
namespace speech {

// An abstract class representing a speech recognizer and it is for a family of
// classes to run event callback.
class SpeechRecognizer {
 public:
  typedef base::Callback<void(const scoped_refptr<dom::Event>&)> EventCallback;

  explicit SpeechRecognizer(const EventCallback& event_callback);

  virtual ~SpeechRecognizer() {}

  virtual void Start(const SpeechRecognitionConfig& config) = 0;
  virtual void Stop() = 0;

 protected:
  void RunEventCallback(const scoped_refptr<dom::Event>& event);

 private:
  // Callback for sending dom events if available.
  EventCallback event_callback_;
};

}  // namespace speech
}  // namespace cobalt

#endif  // COBALT_SPEECH_SPEECH_RECOGNIZER_H_
