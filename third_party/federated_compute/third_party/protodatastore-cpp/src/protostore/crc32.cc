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

#include "protostore/crc32.h"

#include <cstdint>
#include <zlib.h>

#include "absl/strings/string_view.h"

namespace protostore {

namespace {
uint32_t UpdateCrc32(uint32_t crc, const absl::string_view str) {
  if (str.length() > 0) {
    // crc32() already includes a pre- and post-condition of taking the one's
    // complement of the value.
    crc =
        ~crc32(~crc, reinterpret_cast<const Bytef*>(str.data()), str.length());
  }
  return crc;
}
}  // namespace

uint32_t Crc32::Get() const { return crc_; }

uint32_t Crc32::Append(const absl::string_view str) {
  crc_ = UpdateCrc32(crc_, str);
  return crc_;
}

}  // namespace protostore
