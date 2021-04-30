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

#ifndef STARBOARD_SHARED_STARBOARD_SPEECH_RECOGNIZER_SPEECH_RECOGNIZER_INTERNAL_H_
#define STARBOARD_SHARED_STARBOARD_SPEECH_RECOGNIZER_SPEECH_RECOGNIZER_INTERNAL_H_

#include "starboard/shared/internal_only.h"
#include "starboard/speech_recognizer.h"

#if SB_API_VERSION >= 12 || SB_HAS(SPEECH_RECOGNIZER)
struct SbSpeechRecognizerPrivate {
  virtual ~SbSpeechRecognizerPrivate() {}
  virtual bool Start(const SbSpeechConfiguration* configuration) = 0;
  virtual void Stop() = 0;
  virtual void Cancel() = 0;
  static SbSpeechRecognizer CreateSpeechRecognizer(
      const SbSpeechRecognizerHandler* handler);
  static void DestroySpeechRecognizer(SbSpeechRecognizer speech_recognizer);
};
#endif  // SB_API_VERSION >= 12 || SB_HAS(SPEECH_RECOGNIZER)

#endif  // STARBOARD_SHARED_STARBOARD_SPEECH_RECOGNIZER_SPEECH_RECOGNIZER_INTERNAL_H_
