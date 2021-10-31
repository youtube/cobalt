// Copyright 2021 The Cobalt Authors. All Rights Reserved.
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

#include <utility>

#include "starboard/configuration.h"
#include "starboard/types.h"

namespace starboard {
namespace nplb {
namespace {

// Test Structured bindings support, P0217R3

#ifndef __cpp_structured_bindings
#error "Structured bindings support is required"
#endif

constexpr char test_structured_bindings() {
  auto[a, b] = std::pair<int, char>(42, 'A');
  return b;
}

static_assert(test_structured_bindings() == 'A',
              "Structured bindings support is required");

// Test UTF-8 literal support, N4267

#ifndef __cpp_unicode_literals
#error "Unicode literal support is required"
#endif

static_assert('A' == u8'A', "Unicode literal support is required");

// Test constexpr lambda support, P0170R1

constexpr int add_one(int n) {
  return [n] { return n + 1; }();
}

static_assert(add_one(1) == 2, "Constexpr lambdas support is required");

}  // namespace
}  // namespace nplb
}  // namespace starboard
