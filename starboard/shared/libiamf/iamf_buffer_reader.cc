// Copyright 2026 The Cobalt Authors. All Rights Reserved.
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

#include "starboard/shared/libiamf/iamf_buffer_reader.h"

#include <algorithm>
#include <cstring>

#include "starboard/common/log.h"
#include "starboard/common/string.h"

namespace starboard {

IamfBufferReader::IamfBufferReader(const uint8_t* buf, size_t size)
    : buf_(buf), size_(size) {
#if SB_IS_BIG_ENDIAN
#error IamfBufferReader assumes little-endianness.
#endif  // SB_IS_BIG_ENDIAN
}

std::optional<uint8_t> IamfBufferReader::Read1() {
  if (!HasBytes(sizeof(uint8_t))) {
    return std::nullopt;
  }
  return buf_[pos_++];
}

std::optional<uint32_t> IamfBufferReader::Read4() {
  if (!HasBytes(sizeof(uint32_t))) {
    return std::nullopt;
  }
  uint32_t result;
  std::memcpy(&result, &buf_[pos_], sizeof(uint32_t));
  pos_ += sizeof(uint32_t);
  return ByteSwap(result);
}

std::optional<uint32_t> IamfBufferReader::ReadLeb128() {
  uint32_t decoded_value = 0;
  for (size_t i = 0; i < 5; ++i) {
    auto byte_opt = Read1();
    if (!byte_opt.has_value()) {
      // Not enough data.
      return std::nullopt;
    }
    uint8_t byte = *byte_opt;

    if (i == 4 && (byte & 0x7f) > 0x0f) {
      // Invalid 5-byte encoding.
      return std::nullopt;
    }

    decoded_value |= (uint32_t)(byte & 0x7f) << (i * 7);

    if (!(byte & 0x80)) {
      return decoded_value;
    }
  }
  return std::nullopt;  // Value exceeds 5 bytes
}

std::optional<std::string> IamfBufferReader::ReadString() {
  if (!HasBytes(1)) {
    return std::nullopt;
  }

  // The size of the string is capped to 128 bytes.
  // https://aomediacodec.github.io/iamf/v1.0.0-errata.html#convention-data-types
  const size_t kMaxIamfStringSize = 128;
  const size_t max_bytes_to_read =
      std::min(BytesRemaining(), kMaxIamfStringSize);

  const void* null_terminator = memchr(buf_ + pos_, '\0', max_bytes_to_read);
  if (!null_terminator) {
    // No null terminator found within the readable range.
    return std::nullopt;
  }

  const int bytes_read =
      static_cast<const uint8_t*>(null_terminator) - (buf_ + pos_);
  std::string result(reinterpret_cast<const char*>(buf_ + pos_), bytes_read);

  // Account for the null terminator itself in the total bytes read.
  pos_ += bytes_read + 1;
  return result;
}

bool IamfBufferReader::Skip(size_t bytes_to_skip) {
  if (!HasBytes(bytes_to_skip)) {
    return false;
  }
  pos_ += bytes_to_skip;
  return true;
}

const uint8_t* IamfBufferReader::CurrentData() const {
  return buf_ + pos_;
}

size_t IamfBufferReader::BytesRemaining() const {
  return size_ - pos_;
}

size_t IamfBufferReader::size() const {
  return size_;
}

size_t IamfBufferReader::pos() const {
  return pos_;
}

const uint8_t* IamfBufferReader::buf() const {
  return buf_;
}

bool IamfBufferReader::HasBytes(size_t size) const {
  return size + pos_ <= size_;
}

uint32_t IamfBufferReader::ByteSwap(uint32_t x) const {
#if defined(COMPILER_MSVC)
  return _byteswap_ulong(x);
#else
  return __builtin_bswap32(x);
#endif
}

}  // namespace starboard
