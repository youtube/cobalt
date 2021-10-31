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

#ifndef COBALT_SPEECH_SPEECH_RECOGNITION_CONFIG_H_
#define COBALT_SPEECH_SPEECH_RECOGNITION_CONFIG_H_

#include <string>

namespace cobalt {
namespace speech {

// Speech recognition parameters.
struct SpeechRecognitionConfig {
  SpeechRecognitionConfig(const std::string& lang, bool continuous,
                          bool interim_results, uint32 max_alternatives)
      : lang(lang),
        continuous(continuous),
        interim_results(interim_results),
        max_alternatives(max_alternatives) {}

  // This attribute will set the language of the recognition for the request.
  std::string lang;
  // When the continuous attribute is set to false, the user agent MUST return
  // no more than one final result in response to starting recognition. When
  // the continuous attribute is set to true, the user agent MUST return zero
  // or more final results representing multiple consecutive recognitions in
  // response to starting recognition.
  bool continuous;
  // Controls whether interim results are returned.
  bool interim_results;
  // This attribute will set the maximum number of SpeechRecognitionAlternatives
  // per result.
  uint32 max_alternatives;
};

}  // namespace speech
}  // namespace cobalt

#endif  // COBALT_SPEECH_SPEECH_RECOGNITION_CONFIG_H_
