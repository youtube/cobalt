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

#include "media/audio/shell_wav_test_probe.h"

#include <string>

#include "base/file_path.h"
#include "base/file_util.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/platform_file.h"
#include "media/base/endian_util.h"
#include "media/filters/shell_demuxer.h"

// don't include me in release builds please
#if !defined(__LB_SHELL__FOR_RELEASE__)

static const uint16 kWavFormatCodePCM = 0x0001;
static const uint16 kWavFormatCodeIEEEFloat = 0x0003;
// "RIFF" in ASCII (big-endian)
static const uint32 kWav_RIFF = 0x52494646;
// "WAVE" in ASCII (big-endian)
static const uint32 kWav_WAVE = 0x57415645;
// "fmt " in ASCII (big-endian)
static const uint32 kWav_fmt = 0x666d7420;
// "data" in ASCII (big-endian)
static const uint32 kWav_data = 0x64617461;

namespace media {

ShellWavTestProbe::ShellWavTestProbe()
    : wav_file_(NULL),
      form_wav_length_bytes_(kWavTotalHeaderLength - 8),
      format_code_(0),
      channels_(0),
      samples_per_second_(0),
      bits_per_sample_(0),
      bytes_per_frame_(0),
      closed_(true),
      close_after_ms_(0) {}

void ShellWavTestProbe::Initialize(const char* file_name,
                                   int channel_count,
                                   int samples_per_second,
                                   int bits_per_sample,
                                   bool use_floats) {
  // try to open file first
  FilePath base_path;
  bool path_ok = PathService::Get(base::DIR_EXE, &base_path);
  DCHECK(path_ok);
  base_path = base_path.Append(file_name);
  wav_file_ = base::CreatePlatformFile(
      base_path, base::PLATFORM_FILE_CREATE_ALWAYS | base::PLATFORM_FILE_WRITE,
      NULL, NULL);
  DCHECK_NE(wav_file_, base::kInvalidPlatformFileValue);
  closed_ = false;

  if (use_floats)
    format_code_ = kWavFormatCodeIEEEFloat;
  else
    format_code_ = kWavFormatCodePCM;

  channels_ = channel_count;

  bits_per_sample_ = (uint16)bits_per_sample;
  samples_per_second_ = samples_per_second;

  bytes_per_frame_ = (bits_per_sample_ / 8) * channels_;

  // write temporary header, it's incomplete until we know the whole length
  // of the sample stream, but this will advance file pointer to start of
  // audio data
  WriteHeader();
}

// see: http://www-mmsp.ece.mcgill.ca/Documents/AudioFormats/WAVE/WAVE.html
void ShellWavTestProbe::WriteHeader() {
  // first four bytes are FORM RIFF header
  endian_util::store_uint32_big_endian(kWav_RIFF, wav_header_buffer_);
  // next for are length of file - FORM header, uint32 little-endian
  endian_util::store_uint32_little_endian(form_wav_length_bytes_,
                                          wav_header_buffer_ + 4);
  // then WAVE header
  endian_util::store_uint32_big_endian(kWav_WAVE, wav_header_buffer_ + 8);
  // start common chunk with format "fmt " header
  endian_util::store_uint32_big_endian(kWav_fmt, wav_header_buffer_ + 12);
  // length of format chunk, uint32 little-endian
  endian_util::store_uint32_little_endian(kWavFormatChunkLength - 8,
                                          wav_header_buffer_ + 16);
  // format code, uint16 little-endian
  endian_util::store_uint16_little_endian(format_code_,
                                          wav_header_buffer_ + 20);
  // number of channels, uint16 little-endian
  endian_util::store_uint16_little_endian(channels_, wav_header_buffer_ + 22);
  // sample rate, uint32 little-endian
  endian_util::store_uint32_little_endian(samples_per_second_,
                                          wav_header_buffer_ + 24);
  // average bytes per second, uint32 little-endian, derived
  uint32 bytes_per_second = samples_per_second_ * bytes_per_frame_;
  endian_util::store_uint32_little_endian(bytes_per_second,
                                          wav_header_buffer_ + 28);
  // "block align", reading as bytes per frame, uint16 little-endian
  endian_util::store_uint16_little_endian(bytes_per_frame_,
                                          wav_header_buffer_ + 32);
  // bits per sample, uint16 little-endian
  endian_util::store_uint16_little_endian(bits_per_sample_,
                                          wav_header_buffer_ + 34);
  // size of extension format chunk header, uint16 little-endian
  // always 22 bytes for us so we can do > 2 channels audio
  endian_util::store_uint16_little_endian(22, wav_header_buffer_ + 36);
  // valid bits per sample, always same as bits per sample
  endian_util::store_uint16_little_endian(bits_per_sample_,
                                          wav_header_buffer_ + 38);
  // channel mask, 4 bytes, set to all zeroes to keep default channel layout
  endian_util::store_uint32_little_endian(0, wav_header_buffer_ + 40);
  // subformat guid, 16 bytes, first two bytes are format code again, rest
  // are a magic number 00 00 00 00 10 00   80 00 00 aa 00 38 9b 71
  uint64 magic_msd = ((uint64)format_code_ << 48) | 0x0000000000001000;
  endian_util::store_uint64_big_endian(magic_msd, wav_header_buffer_ + 44);
  endian_util::store_uint64_big_endian(0x800000aa00389b71,
                                       wav_header_buffer_ + 52);
  // start the data chunk with "data" header
  endian_util::store_uint32_big_endian(kWav_data, wav_header_buffer_ + 60);
  // data chunk size is form wav length minus the rest of the header bytes
  // uint32 little-endian
  uint32 data_chunk_size = form_wav_length_bytes_ - (kWavTotalHeaderLength - 8);
  endian_util::store_uint32_little_endian(data_chunk_size,
                                          wav_header_buffer_ + 64);
  // alright, aiff header buffer is current, now we can write it into the file
  // jump to start of file
  int result =
      base::SeekPlatformFile(wav_file_, base::PLATFORM_FILE_FROM_BEGIN, 0);
  // write buffer
  result = base::WritePlatformFileAtCurrentPos(
      wav_file_, reinterpret_cast<const char*>(wav_header_buffer_),
      kWavTotalHeaderLength);
  DCHECK_EQ(result, kWavTotalHeaderLength);
}

void ShellWavTestProbe::CloseAfter(uint64 milliseconds) {
  close_after_ms_ = milliseconds;
}

void ShellWavTestProbe::AddData(const uint8* data,
                                uint32 length,
                                uint64 timestamp) {
#if defined(__LB_SHELL__BIG_ENDIAN__) || \
    (defined(OS_STARBOARD) && defined(SB_IS_BIG_ENDIAN) && SB_IS_BIG_ENDIAN)
  uint8* reverse_buffer = (uint8*)malloc(length);
  uint16 bytes_per_sample = bits_per_sample_ / 8;
  int num_words = length / bytes_per_sample;
  for (int i = 0; i < num_words; i++) {
    uint8* out = reverse_buffer + (i * bytes_per_sample);
    if (bytes_per_sample == 2) {
      endian_util::store_uint16_little_endian(((uint16*)data)[i], out);
    } else if (bytes_per_sample == 4) {
      endian_util::store_uint32_little_endian(((uint32*)data)[i], out);
    } else {
      DLOG(ERROR) << "Failed to add data";
    }
  }
  AddDataLittleEndian(reverse_buffer, length, timestamp);
  free(reverse_buffer);
#else
  AddDataLittleEndian(data, length, timestamp);
#endif
}

void ShellWavTestProbe::AddDataLittleEndian(const uint8* data,
                                            uint32 length,
                                            uint64 timestamp) {
  if (closed_)
    return;
  if (!length)
    return;

  int result = base::WritePlatformFileAtCurrentPos(
      wav_file_, reinterpret_cast<const char*>(data), length);
  DCHECK_EQ(result, length);
  base::FlushPlatformFile(wav_file_);

  // update our counters
  form_wav_length_bytes_ += length;

  if (close_after_ms_ > 0) {
    if (timestamp == 0) {
      // guess at timestamp based on total file size
      timestamp = (((uint64)form_wav_length_bytes_ -
                    (uint64)(kWavTotalHeaderLength - 8)) *
                   1000ULL) /
                  (uint64)(samples_per_second_ * bytes_per_frame_);
    }
    if (timestamp > close_after_ms_) {
      Close();
    }
  }
}

void ShellWavTestProbe::AddData(const scoped_refptr<Buffer>& buffer) {
  uint64 timestamp = 0;
  if (buffer->GetTimestamp() != kNoTimestamp()) {
    timestamp = buffer->GetTimestamp().InMilliseconds();
  }
  AddData(buffer->GetData(), buffer->GetDataSize(), timestamp);
}

void ShellWavTestProbe::Close() {
  if (closed_)
    return;

  closed_ = true;
  // write the header again now that we know the lengths
  WriteHeader();
  // close the file
  base::ClosePlatformFile(wav_file_);
  wav_file_ = NULL;
}

}  // namespace media

#endif  // __LB_SHELL__FOR_RELEASE__
