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

#ifndef COBALT_AUDIO_AUDIO_FILE_READER_WAV_H_
#define COBALT_AUDIO_AUDIO_FILE_READER_WAV_H_

#include "cobalt/audio/audio_file_reader.h"

namespace cobalt {
namespace audio {

// WAVE Audio File Format Specifications:
//   http://www-mmsp.ece.mcgill.ca/Documents/AudioFormats/WAVE/WAVE.html
class AudioFileReaderWAV : public AudioFileReader {
 public:
  static scoped_ptr<AudioFileReader> TryCreate(const uint8* data, size_t size);

  scoped_array<uint8> sample_data() OVERRIDE { return sample_data_.Pass(); }
  float sample_rate() const OVERRIDE { return sample_rate_; }
  int32 number_of_frames() const OVERRIDE { return number_of_frames_; }
  int32 number_of_channels() const OVERRIDE { return number_of_channels_; }

 private:
  AudioFileReaderWAV(const uint8* data, size_t size);

  bool ParseRIFFHeader(const uint8* data, size_t size);
  void ParseChunks(const uint8* data, size_t size);
  bool ParseWAV_fmt(const uint8* data, size_t offset, size_t size,
                    bool* is_sample_in_float);
  bool ParseWAV_data(const uint8* data, size_t offset, size_t size,
                     bool is_sample_in_float);

  bool is_valid() { return sample_data_ != NULL; }

  scoped_array<uint8> sample_data_;
  float sample_rate_;
  int32 number_of_frames_;
  int32 number_of_channels_;
};

}  // namespace audio
}  // namespace cobalt

#endif  // COBALT_AUDIO_AUDIO_FILE_READER_WAV_H_
