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

#include "base/logging.h"
#include "media/filters/shell_demuxer.h"

// don't include me in release builds please
#if !defined(__LB_SHELL__FOR_RELEASE__)

extern const std::string *global_game_content_path;

static const uint16 kWavFormatCodePCM = 0x0001;
static const uint16 kWavFormatCodeIEEEFloat = 0x0003;

namespace media {

ShellWavTestProbe::ShellWavTestProbe()
    : wav_file_(NULL)
    , form_wav_length_bytes_(kWavTotalHeaderLength - 8)
    , format_code_(0)
    , samples_per_second_(0)
    , channels_(0)
    , bits_per_sample_(0)
    , closed_(true)
    , close_after_ms_(0)
    , bytes_per_frame_(0) {
}

void ShellWavTestProbe::Initialize(const char* file_name,
                                   int channel_count,
                                   int samples_per_second,
                                   int bits_per_sample,
                                   bool use_floats) {
  // try to open file first
  std::string path = *global_game_content_path + "/" + std::string(file_name);
  wav_file_ = fopen(path.c_str(), "wb");
  DCHECK(wav_file_);
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
  wav_header_buffer_[ 0] = 'R';
  wav_header_buffer_[ 1] = 'I';
  wav_header_buffer_[ 2] = 'F';
  wav_header_buffer_[ 3] = 'F';
  // next for are length of file - FORM header, uint32 little-endian
  wav_header_buffer_[ 4] = (uint8)(form_wav_length_bytes_);
  wav_header_buffer_[ 5] = (uint8)(form_wav_length_bytes_ >> 8);
  wav_header_buffer_[ 6] = (uint8)(form_wav_length_bytes_ >> 16);
  wav_header_buffer_[ 7] = (uint8)(form_wav_length_bytes_ >> 24);
  // then WAVE header
  wav_header_buffer_[ 8] = 'W';
  wav_header_buffer_[ 9] = 'A';
  wav_header_buffer_[10] = 'V';
  wav_header_buffer_[11] = 'E';
  // start common chunk with format "fmt " header
  wav_header_buffer_[12] = 'f';
  wav_header_buffer_[13] = 'm';
  wav_header_buffer_[14] = 't';
  wav_header_buffer_[15] = ' ';
  // length of format chunk, uint32 little-endian
  wav_header_buffer_[16] = (uint8)((kWavFormatChunkLength - 8));
  wav_header_buffer_[17] = (uint8)((kWavFormatChunkLength - 8) >> 8);
  wav_header_buffer_[18] = (uint8)((kWavFormatChunkLength - 8) >> 16);
  wav_header_buffer_[19] = (uint8)((kWavFormatChunkLength - 8) >> 24);
  // format code, uint16 little-endian
  wav_header_buffer_[20] = (uint8)(format_code_);
  wav_header_buffer_[21] = (uint8)(format_code_ >> 8);
  // number of channels, uint16 little-endian
  wav_header_buffer_[22] = (uint8)(channels_);
  wav_header_buffer_[23] = (uint8)(channels_ >> 8);
  // sample rate, uint32 little-endian
  wav_header_buffer_[24] = (uint8)(samples_per_second_);
  wav_header_buffer_[25] = (uint8)(samples_per_second_ >> 8);
  wav_header_buffer_[26] = (uint8)(samples_per_second_ >> 16);
  wav_header_buffer_[27] = (uint8)(samples_per_second_ >> 24);
  // average bytes per second, uint32 little-endian, derived
  uint32 bytes_per_second = samples_per_second_ * bytes_per_frame_;
  wav_header_buffer_[28] = (uint8)(bytes_per_second);
  wav_header_buffer_[29] = (uint8)(bytes_per_second >> 8);
  wav_header_buffer_[30] = (uint8)(bytes_per_second >> 16);
  wav_header_buffer_[31] = (uint8)(bytes_per_second >> 24);
  // "block align", reading as bytes per frame, uint16 little-endian
  wav_header_buffer_[32] = (uint8)(bytes_per_frame_);
  wav_header_buffer_[33] = (uint8)(bytes_per_frame_ >> 8);
  // bits per sample, uint16 little-endian
  wav_header_buffer_[34] = (uint8)(bits_per_sample_);
  wav_header_buffer_[35] = (uint8)(bits_per_sample_ >> 8);
  // size of extension format chunk header, uint16 little-endian
  // always 22 bytes for us so we can do > 2 channels audio
  wav_header_buffer_[36] = 22;
  wav_header_buffer_[37] = 0;
  // valid bits per sample, always same as bits per sample
  wav_header_buffer_[38] = (uint8)(bits_per_sample_);
  wav_header_buffer_[39] = (uint8)(bits_per_sample_ >> 8);
  // channel mask, 4 bytes, set to all zeroes to keep default channel layout
  wav_header_buffer_[40] = 0;
  wav_header_buffer_[41] = 0;
  wav_header_buffer_[42] = 0;
  wav_header_buffer_[43] = 0;
  // subformat guid, 16 bytes, first two bytes are format code again, rest
  // are a magic number 00 00 00 00 10 00 80 00 00 aa 00 38 9b 71
  wav_header_buffer_[44] = (uint8)(format_code_);
  wav_header_buffer_[45] = (uint8)(format_code_ >> 8);
  wav_header_buffer_[46] = 0x00;
  wav_header_buffer_[47] = 0x00;
  wav_header_buffer_[48] = 0x00;
  wav_header_buffer_[49] = 0x00;
  wav_header_buffer_[50] = 0x10;
  wav_header_buffer_[51] = 0x00;
  wav_header_buffer_[52] = 0x80;
  wav_header_buffer_[53] = 0x00;
  wav_header_buffer_[54] = 0x00;
  wav_header_buffer_[55] = 0xaa;
  wav_header_buffer_[56] = 0x00;
  wav_header_buffer_[57] = 0x38;
  wav_header_buffer_[58] = 0x9b;
  wav_header_buffer_[59] = 0x71;
  // start the data chunk with "data" header
  wav_header_buffer_[60] = 'd';
  wav_header_buffer_[61] = 'a';
  wav_header_buffer_[62] = 't';
  wav_header_buffer_[63] = 'a';
  // data chunk size is form wav length minus the rest of the header bytes
  // uint32 little-endian
  uint32 data_chunk_size = form_wav_length_bytes_ - (kWavTotalHeaderLength - 8);
  wav_header_buffer_[64] = (uint8)(data_chunk_size);
  wav_header_buffer_[65] = (uint8)(data_chunk_size >> 8);
  wav_header_buffer_[66] = (uint8)(data_chunk_size >> 16);
  wav_header_buffer_[67] = (uint8)(data_chunk_size >> 24);
  // alright, aiff header buffer is current, now we can write it into the file
  // jump to start of file
  int result = fseek(wav_file_, 0, SEEK_SET);
  DCHECK_EQ(result, 0);
  // write buffer
  result = fwrite(wav_header_buffer_, 1, kWavTotalHeaderLength, wav_file_);
  DCHECK_EQ(result, kWavTotalHeaderLength);
}

void ShellWavTestProbe::CloseAfter(uint64 milliseconds) {
  close_after_ms_ = milliseconds;
}

void ShellWavTestProbe::AddData(const uint8* data,
                                uint32 length,
                                uint64 timestamp) {
  if (closed_) return;
  if (!length) return;

#if defined(__LB_SHELL__BIG_ENDIAN__)
  // assuming 4 bytes per sample on big-endian platforms
  DCHECK_EQ(bits_per_sample_, 32);
  uint8* reverse_buffer = (uint8*)malloc(length);
  int num_words = length / 4;
  uint8* out = reverse_buffer;
  const uint32* input_buffer = (const uint32*)data;
  for (int i = 0; i < num_words; i++) {
    uint32 in_word = *input_buffer;
    *out = (uint8)(in_word);
    out++;
    *out = (uint8)(in_word >> 8);
    out++;
    *out = (uint8)(in_word >> 16);
    out++;
    *out = (uint8)(in_word >> 24);
    out++;
    input_buffer++;
  }
  fwrite(reverse_buffer, 1, length, wav_file_);
  free(reverse_buffer);
#else
  // write the buffer
  fwrite(data, 1, length, wav_file_);
#endif

  fflush(wav_file_);

  // update our counters
  form_wav_length_bytes_ += length;

  if (close_after_ms_ > 0) {
    if (timestamp == 0) {
      // guess at timestamp based on total file size
      timestamp = (((uint64)form_wav_length_bytes_ -
                   (uint64)(kWavTotalHeaderLength - 8)) * 1000ULL) /
                  (uint64)(samples_per_second_ * bytes_per_frame_);
    }
    if (timestamp > close_after_ms_) {
      Close();
    }
  }
}

void ShellWavTestProbe::AddData(scoped_refptr<DataBuffer> data_buffer) {
  uint64 timestamp = 0;
  if (data_buffer->GetTimestamp() != kNoTimestamp()) {
    timestamp = data_buffer->GetTimestamp().InMilliseconds();
  }
  AddData(data_buffer->GetData(),
          data_buffer->GetDataSize(),
          timestamp);
}

void ShellWavTestProbe::Close() {
  closed_ = true;
  // write the header again now that we know the lengths
  WriteHeader();
  // close the file
  fclose(wav_file_);
  wav_file_ = NULL;
}

}  // namespace media

#endif  // __LB_SHELL__FOR_RELEASE__
