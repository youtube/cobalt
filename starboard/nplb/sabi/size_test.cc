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

SB_COMPILE_ASSERT(sizeof(char) == SB_SIZE_OF_CHAR,
                  SB_SIZE_OF_CHAR_is_inconsistent_with_sizeof_char);

SB_COMPILE_ASSERT(sizeof(double) == SB_SIZE_OF_DOUBLE,
                  SB_SIZE_OF_DOUBLE_is_inconsistent_with_sizeof_double);

SB_COMPILE_ASSERT(sizeof(float) == SB_SIZE_OF_FLOAT,
                  SB_SIZE_OF_FLOAT_is_inconsistent_with_sizeof_float);

SB_COMPILE_ASSERT(sizeof(int) == SB_SIZE_OF_INT,
                  SB_SIZE_OF_INT_is_inconsistent_with_sizeof_int);

SB_COMPILE_ASSERT(sizeof(int*) == SB_SIZE_OF_POINTER,
                  SB_SIZE_OF_POINTER_is_inconsistent_with_sizeof_intptr_t);

SB_COMPILE_ASSERT(sizeof(long) == SB_SIZE_OF_LONG,  // NOLINT(runtime/int)
                  SB_SIZE_OF_LONG_is_inconsistent_with_sizeof_long);

SB_COMPILE_ASSERT(sizeof(long long) == SB_SIZE_OF_LLONG,  // NOLINT(runtime/int)
                  SB_SIZE_OF_LONG_is_inconsistent_with_sizeof_llong);

SB_COMPILE_ASSERT(sizeof(short) == SB_SIZE_OF_SHORT,  // NOLINT(runtime/int)
                  SB_SIZE_OF_SHORT_is_inconsistent_with_sizeof_short);

}  // namespace
}  // namespace sabi
}  // namespace starboard

#endif  // SB_API_VERSION >= 12
