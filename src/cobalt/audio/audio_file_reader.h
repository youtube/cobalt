/*
 * Copyright 2015 Google Inc. All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef COBALT_AUDIO_AUDIO_FILE_READER_H_
#define COBALT_AUDIO_AUDIO_FILE_READER_H_

#include "base/memory/scoped_ptr.h"  // For scoped_array

namespace cobalt {
namespace audio {

class AudioFileReader {
 public:
  virtual ~AudioFileReader() {}

  static scoped_ptr<AudioFileReader> TryCreate(const uint8* data, size_t size);

  // Returns the sample data stored as float sample in planar form.  Note that
  // this function transfers the ownership of the data to the caller so it can
  // only be called once.
  virtual scoped_array<uint8> sample_data() = 0;
  virtual float sample_rate() const = 0;
  virtual int32 number_of_frames() const = 0;
  virtual int32 number_of_channels() const = 0;
};

}  // namespace audio
}  // namespace cobalt

#endif  // COBALT_AUDIO_AUDIO_FILE_READER_H_
