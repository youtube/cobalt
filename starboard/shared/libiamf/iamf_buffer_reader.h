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

#ifndef STARBOARD_SHARED_LIBIAMF_IAMF_BUFFER_READER_H_
#define STARBOARD_SHARED_LIBIAMF_IAMF_BUFFER_READER_H_

#include <cstdint>
#include <optional>
#include <string>

namespace starboard {

// A lightweight, forward-only reader for a raw byte buffer. This class
// provides a unified interface for parsing IAMF data. All `Read` methods
// return an `std::optional` to indicate whether the read was successful.
class IamfBufferReader {
 public:
  IamfBufferReader(const uint8_t* buf, size_t size);

  // Reads a single byte.
  std::optional<uint8_t> Read1();
  // Reads a 4-byte big-endian integer.
  std::optional<uint32_t> Read4();
  // Reads a LEB128-encoded unsigned integer.
  std::optional<uint32_t> ReadLeb128();
  // Reads a null-terminated string.
  std::optional<std::string> ReadString();

  // Skips a specified number of bytes. Returns false if there isn't enough
  // data.
  bool Skip(size_t bytes_to_skip);

  // Returns a pointer to the current read position in the buffer.
  const uint8_t* CurrentData() const;
  // Returns the number of bytes remaining in the buffer.
  size_t BytesRemaining() const;

  // Returns the total size of the buffer.
  size_t size() const;
  // Returns the current read position.
  size_t pos() const;
  // Returns a pointer to the start of the buffer.
  const uint8_t* buf() const;

 private:
  bool HasBytes(size_t size) const;
  uint32_t ByteSwap(uint32_t x) const;

  const uint8_t* buf_;
  const size_t size_;
  size_t pos_ = 0;
};

}  // namespace starboard

#endif  // STARBOARD_SHARED_LIBIAMF_IAMF_BUFFER_READER_H_
