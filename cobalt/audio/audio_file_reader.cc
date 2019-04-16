// Copyright 2015 The Cobalt Authors. All Rights Reserved.
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

#include <memory>

#include "cobalt/audio/audio_file_reader.h"

#include "cobalt/audio/audio_file_reader_wav.h"

namespace cobalt {
namespace audio {

// static
std::unique_ptr<AudioFileReader> AudioFileReader::TryCreate(
    const uint8* data, size_t size, SampleType sample_type) {
  // Try to create other type of audio file reader.
  return AudioFileReaderWAV::TryCreate(data, size, sample_type);
}

}  // namespace audio
}  // namespace cobalt
