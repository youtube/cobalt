// Copyright 2024 The Cobalt Authors. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the aLicense is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <iostream>

#define COBALT_CLANG_ERROR_MSG "We compile above Starboard targets like Cobalt, Nplb with " \
    "clang17 or higher. If you are building with a non-Cobalt " \
    "provided toolchain you may run into this error. To fix this error" \
    "build libnplb using the Evergreen toolchain : " \
    "cobalt.dev/development/setup-raspi"

#ifdef __clang__
// Check Clang major version required for building Nplb tests.
// Clang major version can be deduced from "clang_revision" in
// starboard/build/config/clang.gni
static_assert(__clang_major__ >= 17, COBALT_CLANG_ERROR_MSG);
#else
static_assert(false, COBALT_CLANG_ERROR_MSG);
#endif  // __clang__
#undef COBALT_CLANG_ERROR_MSG
