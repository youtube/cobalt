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

#ifndef AUDIO_AUDIO_FILE_READER_WAV_H_
#define AUDIO_AUDIO_FILE_READER_WAV_H_

#include <vector>

#include "cobalt/audio/audio_file_reader.h"

namespace cobalt {
namespace audio {

// WAVE Audio File Format Specifications:
//   http://www-mmsp.ece.mcgill.ca/Documents/AudioFormats/WAVE/WAVE.html
class AudioFileReaderWAV : public AudioFileReader {
 public:
  static scoped_ptr<AudioFileReader> TryCreate(const std::vector<uint8>& data);

  const scoped_refptr<AudioBuffer>& audio_buffer() OVERRIDE {
    return audio_buffer_;
  }

 private:
  explicit AudioFileReaderWAV(const std::vector<uint8>& data);

  bool ParseRIFFHeader(const std::vector<uint8>& audio_data);
  void ParseChunks(const std::vector<uint8>& audio_data);
  bool ParseWAV_fmt(const uint8* riff_data, uint32 offset, uint32 size,
                    bool* is_float, int32* channels, float* sample_rate);
  bool ParseWAV_data(const uint8* riff_data, uint32 offset, uint32 size,
                     bool is_float, int32 channels, float sample_rate);

  AudioBuffer::Float32ArrayVector ReadAsFloatToFloat32ArrayVector(
      int32 channels, int32 number_of_frames, const uint8* data_samples);
  AudioBuffer::Float32ArrayVector ReadAsInt16ToFloat32ArrayVector(
      int32 channels, int32 number_of_frames, const uint8* data_samples);

  bool is_valid() { return audio_buffer_ != NULL; }

  scoped_refptr<AudioBuffer> audio_buffer_;
};

}  // namespace audio
}  // namespace cobalt

#endif  // AUDIO_AUDIO_FILE_READER_WAV_H_
