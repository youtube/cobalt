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

#ifndef COBALT_SPEECH_STARBOARD_SPEECH_RECOGNIZER_H_
#define COBALT_SPEECH_STARBOARD_SPEECH_RECOGNIZER_H_

#include <vector>

#include "base/message_loop/message_loop.h"
#include "cobalt/speech/speech_configuration.h"
#include "cobalt/speech/speech_recognition_result_list.h"
#include "cobalt/speech/speech_recognizer.h"

#if defined(SB_USE_SB_SPEECH_RECOGNIZER)

#include "starboard/speech_recognizer.h"

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

  static bool IsSupported();

  void Start(const SpeechRecognitionConfig& config) override;
  void Stop() override;

 private:
  static void OnSpeechDetected(void* context, bool detected);
  void OnRecognizerSpeechDetected(bool detected);
  static void OnError(void* context, SbSpeechRecognizerError error);
  void OnRecognizerError(SbSpeechRecognizerError error);
  static void OnResults(void* context, SbSpeechResult* results,
                        int results_size, bool is_final);
  void OnRecognizerResults(
      std::vector<SpeechRecognitionAlternative::Data>&& results, bool is_final);

  SbSpeechRecognizer speech_recognizer_;

  // Used for accumulating final results.
  SpeechRecognitionResults final_results_;

  // Track the message loop that created this object so that our callbacks can
  // post back to it.
  base::MessageLoop* message_loop_;

  // We have our callbacks post events back to us using weak pointers, in case
  // this object is destroyed while those tasks are in flight.  Note that it
  // is impossible for the callbacks to be called after this object is
  // destroyed, since SbSpeechRecognizerDestroy() ensures this.
  base::WeakPtrFactory<StarboardSpeechRecognizer> weak_factory_;
};

}  // namespace speech
}  // namespace cobalt

#endif  // defined(SB_USE_SB_SPEECH_RECOGNIZER)

#endif  // COBALT_SPEECH_STARBOARD_SPEECH_RECOGNIZER_H_
