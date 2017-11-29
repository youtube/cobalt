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

#ifndef COBALT_SPEECH_MICROPHONE_H_
#define COBALT_SPEECH_MICROPHONE_H_

#include <string>

#include "base/file_path.h"
#include "base/memory/scoped_ptr.h"
#include "base/optional.h"

namespace cobalt {
namespace speech {

// An abstract class is used for interacting platform specific microphone.
class Microphone {
 public:
  struct Options {
    Options()
        : enable_fake_microphone(false),
          external_audio_data(NULL),
          audio_data_size(0) {}

    // Use fake microphone if this flag is set to true.
    bool enable_fake_microphone;
    // The following members are only applicable to the fake microphone.
    //
    // External audio input data for microphone input.
    const uint8* external_audio_data;
    // Audio data size.
    int audio_data_size;
    // Input file path.
    base::optional<FilePath> file_path;
  };

  virtual ~Microphone() {}

  // Opens microphone port and starts recording audio.
  virtual bool Open() = 0;
  // Reads audio data from microphone. The return value is the bytes that were
  // read from microphone. Negative value indicates a read error. |data_size| is
  // the requested read bytes.
  virtual int Read(char* out_data, int data_size) = 0;
  // Closes microphone port and stops recording audio.
  virtual bool Close() = 0;
  // Returns the minimum requested bytes per microphone read.
  virtual int MinMicrophoneReadInBytes() = 0;
  // Returns true if the microphone is valid.
  virtual bool IsValid() = 0;
  // Returns the label representing the microphone.
  virtual const char* Label() = 0;

 protected:
  Microphone() {}
};

}  // namespace speech
}  // namespace cobalt

#endif  // COBALT_SPEECH_MICROPHONE_H_
