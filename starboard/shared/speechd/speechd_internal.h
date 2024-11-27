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

#ifndef STARBOARD_SHARED_SPEECHD_SPEECHD_INTERNAL_H_
#define STARBOARD_SHARED_SPEECHD_SPEECHD_INTERNAL_H_

#include <libspeechd.h>

#include "starboard/common/mutex.h"
#include "starboard/shared/internal_only.h"

namespace starboard {
namespace shared {
namespace speechd {

// |SpeechDispatcher| is a helper class that is used to implement the
// SbSpeechSynthesis* APIs in terms of the Speech Dispatcher C API:
// https://devel.freebsoft.org/doc/speechd/speech-dispatcher.html
//
// Speech Dispatcher is a front-end API for speech synthesis that supports
// several speech synthesis engines. The |SpeechDispatcher| class will use the
// default synthesis engine settings.
class SpeechDispatcher {
 public:
  // Get the SpeechDispatcher singleton.
  static SpeechDispatcher* Get();

  // Sets the output language. |language| should be an RFC 1776 language code.
  // Returns true if the language was set successfully.
  bool SetLanguage(const char* language);

  // Speak the contents of |text|. If some other text is currently being spoken,
  // |text| will be queued up to be spoken after the current speech (and any
  // other queued up speech) has been spoken.
  void Speak(const char* text);

  // Stop speaking and cancel all queued up text. This is a no-op if there is
  // nothing being spoken.
  void Cancel();

 private:
  SpeechDispatcher();
  ~SpeechDispatcher();

  // Initialize and destroy the singleton instance.
  static void Initialize();
  static void Destroy();

  // Prevent multiple threads from calling spd_* APIs concurrently. It wasn't
  // clear from the documentation whether or not the spd_* API is thread-safe
  // or not, so be on the safe side.
  Mutex lock_;

  // An open connection to Speech Dispatcher.
  SPDConnection* connection_;

  // SpeechDispatcher singleton.
  static SpeechDispatcher* instance;
};

}  // namespace speechd
}  // namespace shared
}  // namespace starboard

#endif  // STARBOARD_SHARED_SPEECHD_SPEECHD_INTERNAL_H_
