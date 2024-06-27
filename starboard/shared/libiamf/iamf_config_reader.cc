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
}  // namespace

bool IamfConfigReader::Read(scoped_refptr<InputBuffer> input_buffer) {
  buffer_head_ = 0;
  has_mix_presentation_id_ = false;
  SB_LOG(INFO) << "Input buffer size is " << input_buffer->size();
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

  SB_LOG(INFO) << "End OBU loop";

  data_size_ = input_buffer->size() - config_size_;
  config_obus_.assign(buf, buf + config_size_);
  data_.assign(buf + config_size_, buf + input_buffer->size());
  SB_LOG(INFO) << ::starboard::HexEncode(config_obus_.data(),
                                         config_obus_.size());
  SB_LOG(INFO) << "Return read";

  return true;
}

bool IamfConfigReader::ReadOBU(const uint8_t* buf, bool& completed_parsing) {
  uint8_t obu_type = 0;
  uint32_t obu_size = 0;
  uint32_t header_size = 0;
  if (!ReadOBUHeader(buf, &obu_type, &obu_size, &header_size)) {
    SB_LOG(ERROR) << "Error reading OBU header";
    return false;
  }
  SB_LOG(INFO) << "OBU size is " << obu_size << " with current buffer head "
               << buffer_head_;

  // const uint8_t* last_byte = buf + buffer_head_ + obu_size;
  int next_obu_pos = buffer_head_ + obu_size;
  int bytes_read = 0;

  switch (static_cast<int>(obu_type)) {
    case kObuTypeCodecConfig:
      SB_LOG(INFO) << "Reading codec config OBU";
      break;
    case kObuTypeAudioElement:
      SB_LOG(INFO) << "Reading audio element OBU";
      break;
    case kObuTypeSequenceHeader:
      SB_LOG(INFO) << "Reading sequence header OBU";
      break;
    case kObuTypeMixPresentation:
      SB_LOG(INFO) << "Reading mix presentation OBU";
      has_mix_presentation_id_ = true;
      bytes_read = ReadLeb128Value(&buf[buffer_head_], &mix_presentation_id_);
      if (bytes_read < 0) {
        return false;
      }
      buffer_head_ += bytes_read;
      break;
    default:
      SB_LOG(INFO) << "OBU type " << static_cast<int>(obu_type);
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
  SB_LOG(INFO) << "buffer head is " << buffer_head_;
  uint8_t header_flags = buf[buffer_head_];
  *obu_type = (header_flags >> 3) & 0x1f;
  buffer_head_++;

  const bool obu_redundant_copy = (header_flags >> 2) & 1;
  const bool obu_trimming_status_flag = (header_flags >> 1) & 1;
  const bool obu_extension_flag = header_flags & 1;

  SB_LOG(INFO) << static_cast<int>(*obu_type);

  *header_size = 1;

  // redundant_copy |= obu_redundant_copy;

  *obu_size = 0;
  int bytes_read = ReadLeb128Value(&buf[buffer_head_], obu_size);
  if (bytes_read < 0) {
    SB_LOG(INFO) << "Error reading OBU size";
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
