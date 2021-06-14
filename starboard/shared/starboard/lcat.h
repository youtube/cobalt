// Copyright 2015 The Cobalt Authors. All Rights Reserved.
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

// Some private implementation utility functions to make string copying fun. May
// be used by any Starboard implementation.

#ifndef STARBOARD_SHARED_STARBOARD_LCAT_H_
#define STARBOARD_SHARED_STARBOARD_LCAT_H_

#include "starboard/shared/internal_only.h"
#include "starboard/shared/starboard/lcpy.h"
#include "starboard/types.h"

#if SB_API_VERSION < 13

namespace starboard {
namespace shared {
namespace starboard {

template <typename CHAR>
int lcatT(CHAR* dst, const CHAR* src, int dst_size) {
  int dst_length = 0;
  for (; dst_length < dst_size; ++dst_length) {
    if (dst[dst_length] == 0)
      break;
  }

  return lcpyT<CHAR>(dst + dst_length, src, dst_size - dst_length) + dst_length;
}

}  // namespace starboard
}  // namespace shared
}  // namespace starboard

#endif  // SB_API_VERSION < 13

#endif  // STARBOARD_SHARED_STARBOARD_LCAT_H_
