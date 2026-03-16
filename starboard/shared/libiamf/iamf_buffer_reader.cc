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

bool IamfBufferReader::Read1(uint8_t* ptr) {
  if (!HasBytes(sizeof(uint8_t)) || !ptr) {
    return false;
  }
  *ptr = buf_[pos_++];
  return true;
}

bool IamfBufferReader::Read4(uint32_t* ptr) {
  if (!HasBytes(sizeof(uint32_t)) || !ptr) {
    return false;
  }
  std::memcpy(ptr, &buf_[pos_], sizeof(uint32_t));
  *ptr = ByteSwap(*ptr);
  pos_ += sizeof(uint32_t);
  return true;
}

bool IamfBufferReader::ReadLeb128(uint32_t* ptr) {
  if (!HasBytes(1) || !ptr) {
    return false;
  }
  int bytes_read = ReadLeb128Internal(
      buf_ + pos_, ptr,
      std::min(static_cast<uint64_t>(BytesRemaining()), sizeof(uint32_t)));
  if (bytes_read < 0) {
    return false;
  }
  pos_ += bytes_read;
  return true;
}

bool IamfBufferReader::ReadString(std::string* str) {
  if (!HasBytes(1) || !str) {
    return false;
  }

  // The size of the string is capped to 128 bytes.
  // https://aomediacodec.github.io/iamf/v1.0.0-errata.html#convention-data-types
  const size_t kMaxIamfStringSize = 128;
  int bytes_read = ReadStringInternal(
      buf_ + pos_, str, std::min(BytesRemaining(), kMaxIamfStringSize));
  if (bytes_read < 0) {
    return false;
  }
  pos_ += bytes_read;
  return true;
}

bool IamfBufferReader::SkipLeb128() {
  uint32_t val;
  return ReadLeb128(&val);
}

bool IamfBufferReader::SkipString() {
  std::string str;
  return ReadString(&str);
}

std::optional<uint8_t> IamfBufferReader::ReadByte() {
  uint8_t byte;
  if (!Read1(&byte)) {
    return std::nullopt;
  }
  return byte;
}

std::optional<uint32_t> IamfBufferReader::ReadLeb128() {
  uint32_t decoded_value = 0;
  for (size_t i = 0; i < 5; ++i) {
    auto byte_opt = ReadByte();
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

bool IamfBufferReader::Skip(size_t bytes_to_skip) {
  return SkipBytes(bytes_to_skip);
}

const uint8_t* IamfBufferReader::CurrentData() const {
  return buf_ + pos_;
}

size_t IamfBufferReader::BytesRemaining() const {
  return size_ - pos_;
}

bool IamfBufferReader::SkipBytes(size_t size) {
  if (!HasBytes(size)) {
    return false;
  }
  pos_ += size;
  return true;
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

inline uint32_t IamfBufferReader::ByteSwap(uint32_t x) const {
#if defined(COMPILER_MSVC)
  return _byteswap_ulong(x);
#else
  return __builtin_bswap32(x);
#endif
}

int IamfBufferReader::ReadLeb128Internal(const uint8_t* buf,
                                         uint32_t* value,
                                         const int max_bytes_to_read) {
  SB_DCHECK(buf);
  SB_DCHECK(value);

  *value = 0;
  bool error = true;
  size_t i = 0;
  for (; i < max_bytes_to_read; ++i) {
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

int IamfBufferReader::ReadStringInternal(const uint8_t* buf,
                                         std::string* str,
                                         int max_bytes_to_read) {
  SB_DCHECK(buf);
  SB_DCHECK(str);

  str->clear();
  str->resize(max_bytes_to_read);

  int bytes_read = ::starboard::strlcpy(
      str->data(), reinterpret_cast<const char*>(buf), max_bytes_to_read);
  if (bytes_read == max_bytes_to_read) {
    // Ensure that the string is null terminated.
    if (buf[bytes_read] != '\0') {
      return -1;
    }
  }
  str->resize(bytes_read);

  // Account for null terminator.
  return ++bytes_read;
}

}  // namespace starboard
