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

#ifndef COBALT_SPEECH_SPEECH_SYNTHESIS_VOICE_H_
#define COBALT_SPEECH_SPEECH_SYNTHESIS_VOICE_H_

#include <string>

#include "base/basictypes.h"
#include "cobalt/script/wrappable.h"

namespace cobalt {
namespace speech {

// The speech synthesis voice interface is the scripted web API for defining a
// speech synthesis voice.
//   https://dvcs.w3.org/hg/speech-api/raw-file/4f41ea1126bb/webspeechapi.html#speechsynthesisvoice
class SpeechSynthesisVoice : public script::Wrappable {
 public:
  SpeechSynthesisVoice(const std::string& voice_uri, const std::string& name,
                       const std::string& lang, const bool local_service,
                       const bool default_voice)
      : voice_uri_(voice_uri),
        name_(name),
        lang_(lang),
        local_service_(local_service),
        default_(default_voice) {}
  ~SpeechSynthesisVoice() override {}

  // Readonly Attributes.
  const std::string& voice_uri() const { return voice_uri_; }
  const std::string& name() const { return name_; }
  const std::string& lang() const { return lang_; }
  bool local_service() const { return local_service_; }
  bool attribute_default() const { return default_; }

  DEFINE_WRAPPABLE_TYPE(SpeechSynthesisVoice);

 private:
  const std::string voice_uri_;
  const std::string name_;
  const std::string lang_;
  const bool local_service_;
  const bool default_;

  DISALLOW_COPY_AND_ASSIGN(SpeechSynthesisVoice);
};

}  // namespace speech
}  // namespace cobalt

#endif  // COBALT_SPEECH_SPEECH_SYNTHESIS_VOICE_H_
