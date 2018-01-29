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

#ifndef COBALT_SPEECH_MICROPHONE_STARBOARD_H_
#define COBALT_SPEECH_MICROPHONE_STARBOARD_H_

#include "cobalt/speech/speech_configuration.h"

#if defined(SB_USE_SB_MICROPHONE)

#include <string>

#include "cobalt/speech/microphone.h"
#include "starboard/microphone.h"

namespace cobalt {
namespace speech {

class MicrophoneStarboard : public Microphone {
 public:
  static const int kDefaultSampleRate = 16000;

  MicrophoneStarboard(int sample_rate, int buffer_size_bytes);
  ~MicrophoneStarboard() override;

  bool Open() override;
  int Read(char* out_data, int data_size) override;
  bool Close() override;
  int MinMicrophoneReadInBytes() override {
    return min_microphone_read_in_bytes_;
  }
  bool IsValid() override { return SbMicrophoneIsValid(microphone_); }
  const char* Label() override { return label_.c_str(); }

 private:
  // Minimum requested bytes per microphone read.
  int min_microphone_read_in_bytes_;
  SbMicrophone microphone_;
  std::string label_;
};

}  // namespace speech
}  // namespace cobalt

#endif  // defined(SB_USE_SB_MICROPHONE)
#endif  // COBALT_SPEECH_MICROPHONE_STARBOARD_H_
