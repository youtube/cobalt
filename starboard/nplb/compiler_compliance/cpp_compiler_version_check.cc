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

#ifdef __clang__
// Check Clang major version required for building nplb tests.
// Clang major version can be deduced from "clang_revision" in
// starboard/build/config/clang.gni
static_assert(
    __clang_major__ >= 17,
    "We compile above starboard targets like cobalt, nplb with "
    "clang17 or higher. If you are building non-modularly or with a non-cobalt "
    "provided toolchain you may run into this issue. There are 2 ways around "
    "this : "
    "1) Build libnplb modularly using the evergreen toolchain : "
    "https://developers.google.com/youtube/cobalt/docs/gen/starboard/doc/"
    "evergreen/cobalt_evergreen_reference_port_raspi2#build_instructions"
    "2) Update the toolchain used to build nplb to clang17 or higher "
    "versions.");
#endif  // __clang__
