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

#ifndef STARBOARD_SHARED_LIBIAMF_IAMF_BUFFER_READER_H_
#define STARBOARD_SHARED_LIBIAMF_IAMF_BUFFER_READER_H_

#include <cstdint>
#include <optional>
#include <string>

namespace starboard {

// A lightweight, forward-only reader for a raw byte buffer. This class
// provides a unified interface for parsing IAMF data, combining features
// needed by both the decoder utilities and MIME parsing utilities.
class IamfBufferReader {
 public:
  IamfBufferReader(const uint8_t* buf, size_t size);

  // Methods for parsing IAMF OBUs (used in iamf_decoder_utils.cc).
  bool Read1(uint8_t* ptr);
  bool Read4(uint32_t* ptr);
  bool ReadLeb128(uint32_t* ptr);
  bool ReadString(std::string* str);
  bool SkipLeb128();
  bool SkipString();

  // Methods for parsing IAMF MIME types and headers (used in iamf_util.cc).
  std::optional<uint8_t> ReadByte();
  std::optional<uint32_t> ReadLeb128();
  bool Skip(size_t bytes_to_skip);
  const uint8_t* CurrentData() const;
  size_t BytesRemaining() const;

  // Common utility methods.
  bool SkipBytes(size_t size);
  size_t size() const;
  size_t pos() const;
  const uint8_t* buf() const;

 private:
  bool HasBytes(size_t size) const;
  inline uint32_t ByteSwap(uint32_t x) const;

  static int ReadLeb128Internal(const uint8_t* buf,
                                uint32_t* value,
                                int max_bytes_to_read);
  static int ReadStringInternal(const uint8_t* buf,
                                std::string* str,
                                int max_bytes_to_read);

  const uint8_t* buf_;
  const size_t size_;
  int pos_ = 0;
};

}  // namespace starboard

#endif  // STARBOARD_SHARED_LIBIAMF_IAMF_BUFFER_READER_H_
