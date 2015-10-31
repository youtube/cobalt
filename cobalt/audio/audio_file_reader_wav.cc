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

#include "cobalt/audio/audio_file_reader_wav.h"

// TODO(***REMOVED***): Include path prefix for lb_platform.h. The path prefix of
// lb_platform.h has platform specific information, so it cannot be resolved
// directly.
#include "lb_platform.h"  // NOLINT[build/include]

namespace cobalt {
namespace audio {

namespace {

const uint32 kWAVChunkID_RIFF = 0x52494646;  // 'RIFF'
const uint32 kWAVWaveID_WAVE = 0x57415645;   // 'WAVE'
const uint32 kWAVChunkID_fmt = 0x666d7420;   // 'fmt '
const uint32 kWAVChunkID_data = 0x64617461;  // 'data'

const uint16 kWAVFormatCodePCM = 0x0001;
const uint16 kWAVFormatCodeFloat = 0x0003;

// 4 bytes "RIFF" Chunk id, 4 bytes "RIFF" Chunk size, 4 bytes "WAVE" id,
// at least 24 bytes "fmt" Chunk and at least 8 bytes "data" Chunk.
uint32 kWAVChunkSize = 44;
uint32 kWAVRIFFChunkHeaderSize = 12;
uint32 kWAVfmtChunkHeaderSize = 16;

}  // namespace

// static
scoped_ptr<AudioFileReader> AudioFileReaderWAV::TryCreate(
    const std::vector<uint8>& data) {
  // Need at least the |kWAVChunkSize| bytes for this to be a WAV.
  if (data.size() < kWAVChunkSize) {
    return scoped_ptr<AudioFileReader>();
  }

  scoped_ptr<AudioFileReaderWAV> audio_file_reader_wav(
      new AudioFileReaderWAV(data));

  if (!audio_file_reader_wav->is_valid()) {
    return scoped_ptr<AudioFileReader>();
  }

  return make_scoped_ptr<AudioFileReader>(audio_file_reader_wav.release());
}

AudioFileReaderWAV::AudioFileReaderWAV(const std::vector<uint8>& audio_data) {
  DCHECK_GE(audio_data.size(), kWAVRIFFChunkHeaderSize);

  if (ParseRIFFHeader(audio_data)) {
    ParseChunks(audio_data);
  }
}

bool AudioFileReaderWAV::ParseRIFFHeader(const std::vector<uint8>& audio_data) {
  const uint8* data = &audio_data[0];

  // First 4 bytes need to be the RIFF chunkID.
  if (LB::Platform::load_uint32_big_endian(data) != kWAVChunkID_RIFF) {
    return false;
  }
  // Sanity-check the size. It should be large enough to hold at least the chunk
  // header. Next 4 bytes need to be chunk size.
  uint32 riff_chunk_size = LB::Platform::load_uint32_little_endian(data + 4);
  if (riff_chunk_size > audio_data.size() - 8 || riff_chunk_size < 4) {
    return false;
  }
  // Next 4 bytes need to be the WAVE id, check for that.
  if (LB::Platform::load_uint32_big_endian(data + 8) != kWAVWaveID_WAVE) {
    return false;
  }

  return true;
}

void AudioFileReaderWAV::ParseChunks(const std::vector<uint8>& audio_data) {
  const uint8* riff_data = &audio_data[0];
  uint32 offset = kWAVRIFFChunkHeaderSize;

  bool is_float = false;
  int32 channels = 0;
  float sample_rate = 0.0f;

  // If the WAV file is PCM format, it has two sub-chunks: first one is "fmt"
  // and the second one is "data".
  // TODO(***REMOVED***): support the cases that the WAV file is non-PCM format and the
  // WAV file is extensible format.
  for (int i = 0; i < 2; ++i) {
    // Sub chunk id.
    uint32 chunk_id = LB::Platform::load_uint32_big_endian(riff_data + offset);
    offset += 4;
    // Sub chunk size.
    uint32 chunk_size =
        LB::Platform::load_uint32_little_endian(riff_data + offset);
    offset += 4;

    if (offset + chunk_size > audio_data.size()) {
      DLOG(WARNING) << "Audio data is not enough.";
      break;
    }

    if (chunk_id == kWAVChunkID_fmt && i == 0) {
      if (!ParseWAV_fmt(riff_data, offset, chunk_size, &is_float, &channels,
                        &sample_rate)) {
        DLOG(WARNING) << "Parse fmt chunk failed.";
        break;
      }
    } else if (chunk_id == kWAVChunkID_data && i == 1) {
      if (!ParseWAV_data(riff_data, offset, chunk_size, is_float, channels,
                         sample_rate)) {
        DLOG(WARNING) << "Parse data chunk failed.";
        break;
      }
    } else {
      DLOG(WARNING) << "Malformated audio chunk.";
      break;
    }

    offset += chunk_size;
  }
}

bool AudioFileReaderWAV::ParseWAV_fmt(const uint8* riff_data, uint32 offset,
                                      uint32 size, bool* is_float,
                                      int32* channels, float* sample_rate) {
  // Check size for complete header.
  if (size < kWAVfmtChunkHeaderSize) {
    return false;
  }

  // AudioFormat.
  uint16 format_code =
      LB::Platform::load_uint16_little_endian(riff_data + offset);
  if (format_code != kWAVFormatCodeFloat && format_code != kWAVFormatCodePCM) {
    DLOG(ERROR) << "Bad format on WAV, we only support uncompressed samples!"
                << "Format code: " << format_code;
    return false;
  }

  *is_float = format_code == kWAVFormatCodeFloat;

  // Load channel count.
  uint16 number_of_channels =
      LB::Platform::load_uint16_little_endian(riff_data + offset + 2);
  *channels = number_of_channels;

  // Load sample rate.
  uint32 samples_per_second =
      LB::Platform::load_uint32_little_endian(riff_data + offset + 4);
  *sample_rate = static_cast<float>(samples_per_second);

  // Skip over:
  // uint32 average byterate
  // uint16 block alignment

  // Check sample size, we only support 32 bit floats or 16 bit PCM.
  uint16 bits_per_sample =
      LB::Platform::load_uint16_little_endian(riff_data + offset + 14);
  if ((*is_float && bits_per_sample != 32) ||
      (!*is_float && bits_per_sample != 16)) {
    DLOG(ERROR) << "Bad bits per sample on WAV. "
                << "Bits per sample: " << bits_per_sample;
    return false;
  }

  return true;
}

bool AudioFileReaderWAV::ParseWAV_data(const uint8* riff_data, uint32 offset,
                                       uint32 size, bool is_float,
                                       int32 channels, float sample_rate) {
  const uint8* data_samples = riff_data + offset;

  AudioBuffer::Float32ArrayVector channels_data;
  int32 number_of_frames;

  // Set number of frames based on size of data chunk. And de-interleave the
  // data to the AudioBuffer.
  if (is_float) {
    number_of_frames = static_cast<int32>(size / (4 * channels));
    channels_data = ReadAsFloatToFloat32ArrayVector(channels, number_of_frames,
                                                    data_samples);
  } else {
    number_of_frames = static_cast<int32>(size / (2 * channels));
    channels_data = ReadAsInt16ToFloat32ArrayVector(channels, number_of_frames,
                                                    data_samples);
  }

  audio_buffer_ = new AudioBuffer(sample_rate, number_of_frames, channels_data);

  return true;
}

AudioBuffer::Float32ArrayVector
AudioFileReaderWAV::ReadAsFloatToFloat32ArrayVector(int32 channels,
                                                    int32 number_of_frames,
                                                    const uint8* data_samples) {
  AudioBuffer::Float32ArrayVector channels_data;
  channels_data.reserve(static_cast<uint32>(channels));

  for (int i = 0; i < static_cast<int>(channels); ++i) {
    scoped_refptr<dom::Float32Array> data(
        new dom::Float32Array(static_cast<uint32>(number_of_frames)));

    float* sample =
        const_cast<float*>(reinterpret_cast<const float*>(data_samples)) + i;

    for (int j = 0; j < static_cast<int>(number_of_frames); ++j) {
      uint32 sample_as_uint32 = LB::Platform::load_uint32_little_endian(
          reinterpret_cast<uint8*>(sample));
      data->Set(static_cast<uint32>(j),
                *(reinterpret_cast<float*>(&sample_as_uint32)));
      sample += channels;
    }

    channels_data.push_back(data);
  }

  return channels_data;
}

AudioBuffer::Float32ArrayVector
AudioFileReaderWAV::ReadAsInt16ToFloat32ArrayVector(int32 channels,
                                                    int32 number_of_frames,
                                                    const uint8* data_samples) {
  AudioBuffer::Float32ArrayVector channels_data;
  channels_data.reserve(static_cast<uint32>(channels));

  // WAV PCM data are channel-interleaved 16-bit signed ints, little-endian.
  for (int i = 0; i < static_cast<int>(channels); ++i) {
    scoped_refptr<dom::Float32Array> data(
        new dom::Float32Array(static_cast<uint32>(number_of_frames)));

    int16* sample =
        const_cast<int16*>(reinterpret_cast<const int16*>(data_samples)) + i;

    for (int j = 0; j < static_cast<int>(number_of_frames); ++j) {
      uint16 sample_pcm_unsigned = LB::Platform::load_uint16_little_endian(
          reinterpret_cast<uint8*>(sample));
      int16 sample_pcm = *(reinterpret_cast<int16*>(&sample_pcm_unsigned));
      data->Set(static_cast<uint32>(j),
                static_cast<float>(sample_pcm) / 32768.0f);
      sample += channels;
    }

    channels_data.push_back(data);
  }

  return channels_data;
}

}  // namespace audio
}  // namespace cobalt
