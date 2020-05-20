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

#if SB_API_VERSION >= 12

namespace starboard {
namespace sabi {
namespace {

SB_COMPILE_ASSERT(SB_ALIGNOF(char) == SB_ALIGNMENT_OF_CHAR,
                  SB_ALIGNMENT_OF_CHAR_is_inconsistent_with_SB_ALIGNOF_char);

SB_COMPILE_ASSERT(SB_ALIGNOF(double) == SB_ALIGNMENT_OF_DOUBLE,
                  SB_ALIGNMENT_OF_DOUBLE_is_inconsistent_with_SB_ALIGNOF_double);

SB_COMPILE_ASSERT(SB_ALIGNOF(float) == SB_ALIGNMENT_OF_FLOAT,
                  SB_ALIGNMENT_OF_FLOAT_is_inconsistent_with_SB_ALIGNOF_float);

SB_COMPILE_ASSERT(SB_ALIGNOF(int) == SB_ALIGNMENT_OF_INT,
                  SB_ALIGNMENT_OF_INT_is_inconsistent_with_SB_ALIGNOF_int);

SB_COMPILE_ASSERT(SB_ALIGNOF(int*) == SB_ALIGNMENT_OF_POINTER,
                  SB_ALIGNMENT_OF_POINTER_is_inconsistent_with_SB_ALIGNOF_pointer);

SB_COMPILE_ASSERT(SB_ALIGNOF(long) == SB_ALIGNMENT_OF_LONG,  // NOLINT(runtime/int)
                  SB_ALIGNMENT_OF_LONG_is_inconsistent_with_SB_ALIGNOF_long);

SB_COMPILE_ASSERT(SB_ALIGNOF(long long) == SB_ALIGNMENT_OF_LLONG,  // NOLINT(runtime/int)
                  SB_ALIGNMENT_OF_LLONG_is_inconsistent_with_SB_ALIGNOF_long_long);

SB_COMPILE_ASSERT(SB_ALIGNOF(short) == SB_ALIGNMENT_OF_SHORT,  // NOLINT(runtime/int)
                  SB_ALIGNMENT_OF_SHORT_is_inconsistent_with_SB_ALIGNOF_short);

}  // namespace
}  // namespace sabi
}  // namespace starboard

#endif  // SB_API_VERSION >= 12
