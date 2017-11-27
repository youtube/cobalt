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

#ifndef COBALT_SPEECH_STARBOARD_SPEECH_RECOGNIZER_H_
#define COBALT_SPEECH_STARBOARD_SPEECH_RECOGNIZER_H_

#include "cobalt/speech/speech_configuration.h"
#include "cobalt/speech/speech_recognition_result_list.h"
#include "cobalt/speech/speech_recognizer.h"
#include "starboard/speech_recognizer.h"

#if defined(SB_USE_SB_SPEECH_RECOGNIZER)

namespace cobalt {
namespace speech {

// Controls |SbSpeechRecognizer| to access the device's recognition service and
// receives the speech recognition results from there.
class StarboardSpeechRecognizer : public SpeechRecognizer {
 public:
  typedef SpeechRecognitionResultList::SpeechRecognitionResults
      SpeechRecognitionResults;

  explicit StarboardSpeechRecognizer(const EventCallback& event_callback);
  ~StarboardSpeechRecognizer();

  void Start(const SpeechRecognitionConfig& config) override;
  void Stop() override;

  void OnRecognizerSpeechDetected(bool detected);
  void OnRecognizerError(SbSpeechRecognizerError error);
  void OnRecognizerResults(SbSpeechResult* results, int results_size,
                           bool is_final);

 private:
  SbSpeechRecognizer speech_recognizer_;
  // Used for accumulating final results.
  SpeechRecognitionResults final_results_;
};

}  // namespace speech
}  // namespace cobalt

#endif  // defined(SB_USE_SB_SPEECH_RECOGNIZER)

#endif  // COBALT_SPEECH_STARBOARD_SPEECH_RECOGNIZER_H_
