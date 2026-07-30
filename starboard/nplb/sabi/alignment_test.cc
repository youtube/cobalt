// Copyright 2019 The Cobalt Authors. All Rights Reserved.
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

namespace nplb {
namespace {

static_assert(SB_ALIGNOF(char) == SB_ALIGNMENT_OF_CHAR);
static_assert(SB_ALIGNOF(double) == SB_ALIGNMENT_OF_DOUBLE);
static_assert(SB_ALIGNOF(float) == SB_ALIGNMENT_OF_FLOAT);
static_assert(SB_ALIGNOF(int) == SB_ALIGNMENT_OF_INT);
static_assert(SB_ALIGNOF(int*) == SB_ALIGNMENT_OF_POINTER);
static_assert(SB_ALIGNOF(long) == SB_ALIGNMENT_OF_LONG);  // NOLINT(runtime/int)
static_assert(SB_ALIGNOF(long long) ==                    // NOLINT(runtime/int)
              SB_ALIGNMENT_OF_LLONG);
static_assert(SB_ALIGNOF(short) ==  // NOLINT(runtime/int)
              SB_ALIGNMENT_OF_SHORT);

}  // namespace
}  // namespace nplb
