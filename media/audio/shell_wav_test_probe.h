/*
 * Copyright 2012 Google Inc. All Rights Reserved.
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

#ifndef MEDIA_AUDIO_SHELL_WAV_TEST_PROBE_H_
#define MEDIA_AUDIO_SHELL_WAV_TEST_PROBE_H_

#include "base/platform_file.h"
#include "media/base/buffers.h"

// don't include me in release builds please
#if !defined(__LB_SHELL__FOR_RELEASE__)

static const uint32 kFormWavHeaderLength = 12;
static const uint32 kWavFormatChunkLength = 48;
static const uint32 kWavDataChunkHeaderLength = 8;
static const uint32 kWavTotalHeaderLength =
    kFormWavHeaderLength + kWavFormatChunkLength + kWavDataChunkHeaderLength;

namespace media {

// Utility class for saving decoded audio bytes into a WAV file
class MEDIA_EXPORT ShellWavTestProbe {
 public:
  ShellWavTestProbe();
  // if use_floats is true then the data is written as floating point,
  // if false it is assumed to be PCM unsigned ints
  void Initialize(const char* file_name,
                  int channel_count,
                  int samples_per_second,
                  int bits_per_sample,
                  bool use_floats);
  // automatically close the file after arg milliseconds added to file,
  // Close() will happen on first call to AddData() with timestamp past argument
  void CloseAfter(uint64 milliseconds);
  void AddData(const scoped_refptr<Buffer>& buffer);
  // timestamp can be zero, in which case we will guess at timestamp based on
  // number of bytes written, size of samples, and sample rate
  void AddData(const uint8* data, uint32 length, uint64 timestamp);
  void AddDataLittleEndian(const uint8* data, uint32 length, uint64 timestamp);
  void Close();

 private:
  // take the current state variables below and use them to write the
  // WAV header at the top of the file. Moves the file pointer.
  void WriteHeader();

  base::PlatformFile wav_file_;
  // wav header state variables
  uint32 form_wav_length_bytes_;
  uint16 format_code_;
  uint16 channels_;
  uint32 samples_per_second_;
  uint16 bits_per_sample_;
  uint8 wav_header_buffer_[kWavTotalHeaderLength];
  uint32 bytes_per_frame_;
  // other state
  bool closed_;
  uint64 close_after_ms_;
};

}  // namespace media

#endif  // __LB_SHELL__FOR_RELEASE__

#endif  // MEDIA_AUDIO_SHELL_WAV_TEST_PROBE_H_
