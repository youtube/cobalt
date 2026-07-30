// Copyright (C) 2021 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef PROTOSTORE_CRC32_H_
#define PROTOSTORE_CRC32_H_

#include <cstdint>
#include <zlib.h>

#include "absl/strings/string_view.h"

namespace protostore {

// Efficient mechanism to incrementally compute checksum of a file and keep it
// updated when its content changes. Internally uses zlib based crc32()
// implementation.
//
// See https://www.zlib.net/manual.html#Checksum for more details.
//
// CRC32C::Extend in //util/hash/crc32c.h crashes as described in b/145837799.
class Crc32 {
 public:
  // Default to the checksum of an empty string, that is "0".
  Crc32() : crc_(0) {}

  explicit Crc32(uint32_t init_crc) : crc_(init_crc) {}

  inline bool operator==(const Crc32& other) const {
    return crc_ == other.Get();
  }

  // Returns the checksum of all the data that has been processed till now.
  uint32_t Get() const;

  // Incrementally update the current checksum to reflect the fact that the
  // underlying data has been appended with 'str'. It calculates a new crc32
  // based on the current crc value and the newly appended string.
  //
  // NOTE: As this method accepts incremental appends, all these 3 will lead to
  // the same checksum:
  // 1) crc32.Append("AAA"); crc32.Append("BBB");
  // 2) crc32.Append("AAABBB");
  // 3) crc32.Append("AA"); crc32.Append("AB"); crc32.Append("BB");
  //
  // NOTE: While this class internally uses zlib's crc32(),
  // Crc32(base_crc).Append(str) is not the same as zlib::crc32(base_crc, str);
  uint32_t Append(absl::string_view str);

 private:
  uint32_t crc_;
};

}  // namespace protostore

#endif  // PROTOSTORE_CRC32_H_
