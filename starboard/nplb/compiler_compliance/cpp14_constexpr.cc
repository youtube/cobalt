// Copyright 2020 The Cobalt Authors. All Rights Reserved.
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

#include "starboard/configuration.h"
#include "starboard/types.h"

namespace starboard {
namespace nplb {
namespace {

constexpr const char kPangram[] = "The quick brown fox jumps over a lazy dog";

// The C++14 constexpr permits allocations and branches.
constexpr size_t CompileTimeStringLength(const char* str) {
  size_t length = 0;
  while (*(str + length)) {
    ++length;
  }
  return length + 1;
}

SB_COMPILE_ASSERT(CompileTimeStringLength(kPangram) == sizeof(kPangram),
                  CompileTimeStringLength_does_not_return_correct_length);
}  // namespace
}  // namespace nplb
}  // namespace starboard
