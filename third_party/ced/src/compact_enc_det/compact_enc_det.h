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

#ifndef COMPACT_ENC_DET_COMPACT_ENC_DET_H_
#define COMPACT_ENC_DET_COMPACT_ENC_DET_H_

#if !defined(COBALT_PENDING_CLEAN_UP)
#error "This stub should be cleaned up."
#endif

#include "starboard/common/log.h"

enum Encoding { UNKNOWN_ENCODING };

// constexpr int UNKNOWN_ENCODING = 1;
constexpr int UNKNOWN_LANGUAGE = 2;

struct CompactEncDet {
  enum _foo_unused { QUERY_CORPUS };

  static Encoding DetectEncoding(const char*,
                                 size_t,
                                 std::nullptr_t,
                                 std::nullptr_t,
                                 std::nullptr_t,
                                 Encoding,
                                 const int&,
                                 _foo_unused,
                                 bool,
                                 int*,
                                 bool*) {
    SB_NOTIMPLEMENTED();
    return Encoding::UNKNOWN_ENCODING;
  }
};

std::string MimeEncodingName(Encoding) {
  SB_NOTIMPLEMENTED();
  return "";
}

#endif  // COMPACT_ENC_DET_COMPACT_ENC_DET_H_
