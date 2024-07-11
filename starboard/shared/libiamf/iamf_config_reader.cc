// Copyright 2024 The Cobalt Authors. All Rights Reserved.
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

#include "starboard/shared/libiamf/iamf_config_reader.h"

#include <string>

#include "starboard/common/string.h"

namespace starboard {
namespace shared {
namespace libiamf {

namespace {
// From
// https://aomediacodec.github.io/iamf/v1.0.0-errata.html#obu-header-syntax.
constexpr int kObuTypeCodecConfig = 0;
constexpr int kObuTypeAudioElement = 1;
constexpr int kObuTypeMixPresentation = 2;
constexpr int kObuTypeSequenceHeader = 31;

// From
// https://aomediacodec.github.io/iamf/v1.0.0-errata.html#obu-codecconfig.
constexpr int kFourccOpus = 0x4f707573;
constexpr int kFourccMp4a = 0x6d703461;
constexpr int kFourccFlac = 0x664c6143;
constexpr int kFourccIpcm = 0x6970636d;

inline uint32_t ByteSwap(uint32_t x) {
#if defined(COMPILER_MSVC)
  return _byteswap_ulong(x);
#else
  return __builtin_bswap32(x);
#endif
}

// Decodes an Leb128 value and stores it in |value|. Returns the number of bytes
// read. Returns -1 on error.
int ReadLeb128Value(const uint8_t* buf, uint32_t* value) {
  SB_DCHECK(buf);
  SB_DCHECK(value);
  *value = 0;
  bool error = true;
  size_t i = 0;
  for (; i < sizeof(uint32_t); ++i) {
    uint8_t byte = buf[i];
    *value |= ((byte & 0x7f) << (i * 7));
    if (!(byte & 0x80)) {
      error = false;
      break;
    }
  }

  if (error) {
    return -1;
  }
  return i + 1;
}

int ReadString(const uint8_t* buf, std::string& value) {
  SB_DCHECK(buf);
  value = "";
  value.reserve(128);

  int bytes_read = 0;
  while (bytes_read < 128 && buf[bytes_read] != '\0') {
    value.push_back(static_cast<char>(buf[bytes_read]));
    bytes_read++;
  }

  return bytes_read;
}
}  // namespace

bool IamfConfigReader::Read(scoped_refptr<InputBuffer> input_buffer) {
  Reset();

  const uint8_t* buf = input_buffer->data();
  SB_DCHECK(buf);

  bool completed_parsing = false;
  while (!completed_parsing && buffer_head_ < input_buffer->size()) {
    if (!ReadOBU(buf, completed_parsing)) {
      SB_LOG(INFO) << "Error parsing config OBUs";
      return false;
    }
  }

  SB_CHECK(completed_parsing);

  data_size_ = input_buffer->size() - config_size_;
  config_obus_.assign(buf, buf + config_size_);
  data_.assign(buf + config_size_, buf + input_buffer->size());

  return true;
}

void IamfConfigReader::Reset() {
  buffer_head_ = 0;
  has_mix_presentation_id_ = false;
  mix_presentation_id_ = 0;
  config_size_ = 0;
  data_size_ = 0;
  sample_rate_ = 0;
  samples_per_buffer_ = 0;
  sample_size_ = 0;
}

bool IamfConfigReader::ReadOBU(const uint8_t* buf, bool& completed_parsing) {
  uint8_t obu_type = 0;
  uint32_t obu_size = 0;
  uint32_t header_size = 0;
  if (!ReadOBUHeader(buf, &obu_type, &obu_size, &header_size)) {
    SB_LOG(ERROR) << "Error reading OBU header";
    return false;
  }

  int next_obu_pos = buffer_head_ + obu_size;
  int bytes_read = 0;

  uint32_t count_label = 0;
  std::string str;
  uint32_t num_sub_mixes = 0;

  switch (static_cast<int>(obu_type)) {
    case kObuTypeCodecConfig: {
      sample_rate_ = 0;
      uint32_t codec_config_id;
      bytes_read = ReadLeb128Value(&buf[buffer_head_], &codec_config_id);
      if (bytes_read < 0) {
        return false;
      }
      buffer_head_ += bytes_read;

      uint32_t codec_id = 0;
      std::memcpy(&codec_id, &buf[buffer_head_], sizeof(uint32_t));
      // Mp4 is in big-endian
      codec_id = ByteSwap(codec_id);
      buffer_head_ += 4;

      bytes_read = ReadLeb128Value(&buf[buffer_head_], &samples_per_buffer_);
      if (bytes_read < 0) {
        return false;
      }
      buffer_head_ += bytes_read;

      // audio_roll_distance
      buffer_head_ += 2;

      switch (codec_id) {
        case kFourccOpus:
          sample_rate_ = 48000;
          break;
        case kFourccMp4a:
          // TODO: Properly parse for the sample rate. 48000 is the assumed
          // sample rate.
          sample_rate_ = 48000;
          break;
        case kFourccFlac: {
          // Skip METADATA_BLOCK_HEADER.
          buffer_head_ += 4;
          // Skip first 10 bytes of METADATA_BLOCK_STREAMINFO.
          buffer_head_ += 10;

          std::memcpy(&sample_rate_, &buf[buffer_head_], sizeof(uint32_t));
          sample_rate_ = ByteSwap(sample_rate_);
          sample_rate_ = sample_rate_ >> 12;
          break;
        }
        case kFourccIpcm: {
          // sample_format_flags
          buffer_head_++;
          uint8_t sample_size_byte = buf[buffer_head_];
          sample_size_ = static_cast<int>(sample_size_byte);
          buffer_head_++;

          std::memcpy(&sample_rate_, &buf[buffer_head_], sizeof(uint32_t));
          sample_rate_ = ByteSwap(sample_rate_);
          break;
        }
        default:
          SB_NOTREACHED();
      }

      break;
    }
    case kObuTypeAudioElement:
    case kObuTypeSequenceHeader:
      break;
    case kObuTypeMixPresentation: {
      // TODO: Complete Mix Presentation OBU parsing
      has_mix_presentation_id_ = true;
      bytes_read = ReadLeb128Value(&buf[buffer_head_], &mix_presentation_id_);
      if (bytes_read < 0) {
        return false;
      }
      buffer_head_ += bytes_read;

      // count_label
      bytes_read = ReadLeb128Value(&buf[buffer_head_], &count_label);
      if (bytes_read < 0) {
        return false;
      }
      buffer_head_ += bytes_read;

      // language_label
      for (int i = 0; i < count_label; ++i) {
        buffer_head_ += ReadString(&buf[buffer_head_], str);
      }

      // MixPresentationAnnotations
      for (int i = 0; i < count_label; ++i) {
        buffer_head_ += ReadString(&buf[buffer_head_], str);
      }

      // num_sub_mixes
      bytes_read = ReadLeb128Value(&buf[buffer_head_], &num_sub_mixes);
      if (bytes_read < 0) {
        return false;
      }
      buffer_head_ += bytes_read;

      break;
    }
    default:
      // Once an OBU is read that is not a descriptor, descriptor parsing is
      // assumed to be complete.
      completed_parsing = true;
      break;
  }

  if (!completed_parsing) {
    // Skip to next OBU
    buffer_head_ = next_obu_pos;
    config_size_ += obu_size + header_size;
  }
  return true;
}

bool IamfConfigReader::ReadOBUHeader(const uint8_t* buf,
                                     uint8_t* obu_type,
                                     uint32_t* obu_size,
                                     uint32_t* header_size) {
  uint8_t header_flags = buf[buffer_head_];
  *obu_type = (header_flags >> 3) & 0x1f;
  buffer_head_++;

  const bool obu_redundant_copy = (header_flags >> 2) & 1;
  const bool obu_trimming_status_flag = (header_flags >> 1) & 1;
  const bool obu_extension_flag = header_flags & 1;

  *header_size = 1;

  *obu_size = 0;
  int bytes_read = ReadLeb128Value(&buf[buffer_head_], obu_size);
  if (bytes_read < 0) {
    return false;
  }
  buffer_head_ += bytes_read;
  *header_size += bytes_read;

  if (obu_trimming_status_flag) {
    uint32_t value;
    bytes_read = ReadLeb128Value(&buf[buffer_head_], &value);
    buffer_head_ += bytes_read;
    *header_size += bytes_read;
    *obu_size -= bytes_read;
    bytes_read = ReadLeb128Value(&buf[buffer_head_], &value);
    buffer_head_ += bytes_read;
    *header_size += bytes_read;
    *obu_size -= bytes_read;
  }

  if (obu_extension_flag) {
    uint32_t extension_header_size;
    bytes_read = ReadLeb128Value(&buf[buffer_head_], &extension_header_size);
    buffer_head_ += extension_header_size;
    *obu_size -= extension_header_size;
    *header_size += bytes_read;
  }
  return true;
}

}  // namespace libiamf
}  // namespace shared
}  // namespace starboard
