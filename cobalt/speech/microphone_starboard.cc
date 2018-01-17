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

#include "cobalt/speech/microphone_starboard.h"

#if defined(SB_USE_SB_MICROPHONE)

#include "starboard/log.h"

namespace cobalt {
namespace speech {

namespace {
// The maximum of microphones which can be supported. Currently only supports
// one microphone.
const int kNumberOfMicrophones = 1;

#if SB_API_VERSION >= SB_MICROPHONE_LABEL_API_VERSION
template<std::size_t N>
bool IsNullTerminated(const char(&str)[N]) {
  for (size_t i = 0; i < N; ++i) {
    if (str[i] == '\0') {
      return true;
    }
  }
  return false;
}
#endif

}  // namespace

MicrophoneStarboard::MicrophoneStarboard(int sample_rate, int buffer_size_bytes)
    : Microphone(),
      min_microphone_read_in_bytes_(-1),
      microphone_(kSbMicrophoneInvalid) {
  SbMicrophoneInfo info[kNumberOfMicrophones];
  int microphone_num = SbMicrophoneGetAvailable(info, kNumberOfMicrophones);

  // Loop all the available microphones and create a valid one.
  for (int index = 0; index < microphone_num; ++index) {
    if (!SbMicrophoneIsSampleRateSupported(info[index].id, sample_rate)) {
      continue;
    }

    microphone_ =
        SbMicrophoneCreate(info[index].id, sample_rate, buffer_size_bytes);
    if (!SbMicrophoneIsValid(microphone_)) {
      continue;
    }

    // Created a microphone successfully.
    min_microphone_read_in_bytes_ = info[index].min_read_size;

#if SB_API_VERSION >= SB_MICROPHONE_LABEL_API_VERSION
    if (IsNullTerminated(info[index].label)) {
      label_ = info[index].label;
    }
#endif

    return;
  }
}

MicrophoneStarboard::~MicrophoneStarboard() {
  SbMicrophoneDestroy(microphone_);
}

bool MicrophoneStarboard::Open() {
  SB_DCHECK(SbMicrophoneIsValid(microphone_));
  return SbMicrophoneOpen(microphone_);
}

int MicrophoneStarboard::Read(char* out_data, int data_size) {
  SB_DCHECK(SbMicrophoneIsValid(microphone_));
  return SbMicrophoneRead(microphone_, out_data, data_size);
}

bool MicrophoneStarboard::Close() {
  SB_DCHECK(SbMicrophoneIsValid(microphone_));
  return SbMicrophoneClose(microphone_);
}

}  // namespace speech
}  // namespace cobalt

#endif  // defined(SB_USE_SB_MICROPHONE)
