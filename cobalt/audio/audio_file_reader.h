// Copyright 2015 Google Inc. All Rights Reserved.
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

#ifndef COBALT_AUDIO_AUDIO_FILE_READER_H_
#define COBALT_AUDIO_AUDIO_FILE_READER_H_

#include "base/memory/scoped_ptr.h"
#include "cobalt/audio/audio_helpers.h"

namespace cobalt {
namespace audio {

class AudioFileReader {
 public:
  virtual ~AudioFileReader() {}

  static scoped_ptr<AudioFileReader> TryCreate(const uint8* data, size_t size,
                                               SampleType sample_type);

  virtual float sample_rate() const = 0;
  virtual int32 number_of_frames() const = 0;
  virtual int32 number_of_channels() const = 0;
  virtual SampleType sample_type() const = 0;

  virtual scoped_ptr<ShellAudioBus> ResetAndReturnAudioBus() = 0;
};

}  // namespace audio
}  // namespace cobalt

#endif  // COBALT_AUDIO_AUDIO_FILE_READER_H_
