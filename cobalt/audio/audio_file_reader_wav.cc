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

#include "cobalt/audio/audio_file_reader_wav.h"

#include "base/basictypes.h"
#include "base/logging.h"
#if defined(COBALT_MEDIA_SOURCE_2016)
#include "cobalt/media/base/endian_util.h"
#else  // defined(COBALT_MEDIA_SOURCE_2016)
#include "media/base/endian_util.h"
#endif  // defined(COBALT_MEDIA_SOURCE_2016)
#include "starboard/memory.h"

namespace cobalt {
namespace audio {

#if defined(COBALT_MEDIA_SOURCE_2016)
using media::endian_util::load_uint16_little_endian;
using media::endian_util::load_uint32_big_endian;
using media::endian_util::load_uint32_little_endian;
#else   // defined(COBALT_MEDIA_SOURCE_2016)
using ::media::endian_util::load_uint16_little_endian;
using ::media::endian_util::load_uint32_big_endian;
using ::media::endian_util::load_uint32_little_endian;
#endif  // defined(COBALT_MEDIA_SOURCE_2016)

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
    const uint8* data, size_t size, SampleType sample_type) {
  // Need at least the |kWAVChunkSize| bytes for this to be a WAV.
  if (size < kWAVChunkSize) {
    return scoped_ptr<AudioFileReader>();
  }

  scoped_ptr<AudioFileReaderWAV> audio_file_reader_wav(
      new AudioFileReaderWAV(data, size, sample_type));

  if (!audio_file_reader_wav->is_valid()) {
    return scoped_ptr<AudioFileReader>();
  }

  return make_scoped_ptr<AudioFileReader>(audio_file_reader_wav.release());
}

AudioFileReaderWAV::AudioFileReaderWAV(const uint8* data, size_t size,
                                       SampleType sample_type)
    : sample_rate_(0.f),
      number_of_frames_(0),
      number_of_channels_(0),
      sample_type_(sample_type) {
  DCHECK_GE(size, kWAVRIFFChunkHeaderSize);

  if (ParseRIFFHeader(data, size)) {
    ParseChunks(data, size);
  }
}

bool AudioFileReaderWAV::ParseRIFFHeader(const uint8* data, size_t size) {
  // First 4 bytes need to be the RIFF chunkID.
  if (load_uint32_big_endian(data) != kWAVChunkID_RIFF) {
    return false;
  }
  // Sanity-check the size. It should be large enough to hold at least the chunk
  // header. Next 4 bytes need to be chunk size.
  uint32 riff_chunk_size = load_uint32_little_endian(data + 4);
  if (riff_chunk_size > size - 8 || riff_chunk_size < 4) {
    return false;
  }
  // Next 4 bytes need to be the WAVE id, check for that.
  if (load_uint32_big_endian(data + 8) != kWAVWaveID_WAVE) {
    return false;
  }

  return true;
}

void AudioFileReaderWAV::ParseChunks(const uint8* data, size_t size) {
  uint32 offset = kWAVRIFFChunkHeaderSize;
  bool is_src_sample_in_float = false;
  // If the WAV file is PCM format, it has two sub-chunks: first one is "fmt"
  // and the second one is "data".
  // TODO: support the cases that the WAV file is non-PCM format and the
  // WAV file is extensible format.
  for (int i = 0; i < 2; ++i) {
    // Sub chunk id.
    uint32 chunk_id = load_uint32_big_endian(data + offset);
    offset += 4;
    // Sub chunk size.
    uint32 chunk_size = load_uint32_little_endian(data + offset);
    offset += 4;

    if (offset + chunk_size > size) {
      DLOG(WARNING) << "Audio data is not enough.";
      break;
    }

    if (chunk_id == kWAVChunkID_fmt && i == 0) {
      if (!ParseWAV_fmt(data, offset, chunk_size, &is_src_sample_in_float)) {
        DLOG(WARNING) << "Parse fmt chunk failed.";
        break;
      }
    } else if (chunk_id == kWAVChunkID_data && i == 1) {
      if (!ParseWAV_data(data, offset, chunk_size, is_src_sample_in_float)) {
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

bool AudioFileReaderWAV::ParseWAV_fmt(const uint8* data, size_t offset,
                                      size_t size,
                                      bool* is_src_sample_in_float) {
  DCHECK(is_src_sample_in_float);

  // Check size for complete header.
  if (size < kWAVfmtChunkHeaderSize) {
    return false;
  }

  // AudioFormat.
  uint16 format_code = load_uint16_little_endian(data + offset);
  if (format_code != kWAVFormatCodeFloat && format_code != kWAVFormatCodePCM) {
    DLOG(ERROR) << "Bad format on WAV, we only support uncompressed samples!"
                << "Format code: " << format_code;
    return false;
  }

  *is_src_sample_in_float = format_code == kWAVFormatCodeFloat;

  // Load channel count.
  number_of_channels_ = load_uint16_little_endian(data + offset + 2);
  if (number_of_channels_ == 0) {
    DLOG(ERROR) << "No channel on WAV.";
    return false;
  }

  // Load sample rate.
  sample_rate_ =
      static_cast<float>(load_uint32_little_endian(data + offset + 4));

  // Skip over:
  // uint32 average byterate
  // uint16 block alignment

  // Check sample size, we only support 32 bit floats or 16 bit PCM.
  uint16 bits_per_sample = load_uint16_little_endian(data + offset + 14);
  if ((*is_src_sample_in_float && bits_per_sample != 32) ||
      (!*is_src_sample_in_float && bits_per_sample != 16)) {
    DLOG(ERROR) << "Bad bits per sample on WAV. "
                << "Bits per sample: " << bits_per_sample;
    return false;
  }

  return true;
}

bool AudioFileReaderWAV::ParseWAV_data(const uint8* data, size_t offset,
                                       size_t size,
                                       bool is_src_sample_in_float) {
  // Set number of frames based on size of data chunk.
  const int32 bytes_per_src_sample = static_cast<int32>(
      is_src_sample_in_float ? sizeof(float) : sizeof(int16));
  number_of_frames_ =
      static_cast<int32>(size / (bytes_per_src_sample * number_of_channels_));

  // We store audio samples in the current platform's preferred format.
  audio_bus_.reset(new ShellAudioBus(number_of_channels_, number_of_frames_,
                                     sample_type_,
                                     ShellAudioBus::kInterleaved));

// Both the source data and the destination data are stored in interleaved.
#if SB_IS(LITTLE_ENDIAN)
  if ((!is_src_sample_in_float && sample_type_ == kSampleTypeInt16) ||
      (is_src_sample_in_float && sample_type_ == kSampleTypeFloat32)) {
    SbMemoryCopy(audio_bus_->interleaved_data(), data + offset, size);
  } else if (!is_src_sample_in_float && sample_type_ == kSampleTypeFloat32) {
    // Convert from int16 to float32
    const int16* src_samples = reinterpret_cast<const int16*>(data + offset);
    float* dest_samples =
        reinterpret_cast<float*>(audio_bus_->interleaved_data());

    for (int32 i = 0; i < number_of_frames_ * number_of_channels_; ++i) {
      *dest_samples = ConvertSample<int16, float>(*src_samples);
      ++src_samples;
      ++dest_samples;
    }
  } else {
    // Convert from float32 to int16
    const float* src_samples = reinterpret_cast<const float*>(data + offset);
    int16* dest_samples =
        reinterpret_cast<int16*>(audio_bus_->interleaved_data());

    for (int32 i = 0; i < number_of_frames_ * number_of_channels_; ++i) {
      *dest_samples = ConvertSample<float, int16>(*src_samples);
      ++src_samples;
      ++dest_samples;
    }
  }
#else   // SB_IS(LITTLE_ENDIAN)
  if (!is_src_sample_in_float && sample_type_ == kSampleTypeInt16) {
    const uint8_t* src_samples = data + offset;
    int16* dest_samples =
        reinterpret_cast<int16*>(audio_bus_->interleaved_data());
    for (int32 i = 0; i < number_of_frames_ * number_of_channels_; ++i) {
      *dest_samples = load_uint16_little_endian(src_samples);
      src_samples += bytes_per_src_sample;
      ++dest_samples;
    }
  } else if (is_src_sample_in_float && sample_type_ == kSampleTypeFloat32) {
    const uint8_t* src_samples = data + offset;
    float* dest_samples =
        reinterpret_cast<float*>(audio_bus_->interleaved_data());
    for (int32 i = 0; i < number_of_frames_ * number_of_channels_; ++i) {
      uint32 sample_as_uint32 = load_uint32_little_endian(src_samples);
      *dest_samples = bit_cast<float>(sample_as_uint32);
      src_samples += bytes_per_src_sample;
      ++dest_samples;
    }
  } else if (!is_src_sample_in_float && sample_type_ == kSampleTypeFloat32) {
    // Convert from int16 to float32
    const uint8_t* src_samples = data + offset;
    float* dest_samples =
        reinterpret_cast<float*>(audio_bus_->interleaved_data());
    for (int32 i = 0; i < number_of_frames_ * number_of_channels_; ++i) {
      int16 sample_as_int16 = load_int16_little_endian(src_samples);
      *dest_samples = ConvertSample<int16, float>(sample_as_int16);
      src_samples += bytes_per_src_sample;
      ++dest_samples;
    }
  } else {
    // Convert from float32 to int16
    const uint8_t* src_samples = data + offset;
    int16* dest_samples =
        reinterpret_cast<int16*>(audio_bus_->interleaved_data());
    for (int32 i = 0; i < number_of_frames_ * number_of_channels_; ++i) {
      uint32 sample_as_uint32 = load_uint32_little_endian(src_samples);
      *dest_samples =
          ConvertSample<float, int16>(bit_cast<float>(sample_as_uint32));
      src_samples += bytes_per_src_sample;
      ++dest_samples;
    }
  }
#endif  // SB_IS(LITTLE_ENDIAN)

  return true;
}

}  // namespace audio
}  // namespace cobalt
