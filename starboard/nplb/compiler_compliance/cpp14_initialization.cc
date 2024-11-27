// Copyright 2021 The Cobalt Authors. All Rights Reserved.
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

#include <vector>
#include "starboard/configuration.h"
#include "starboard/types.h"

namespace starboard {
namespace nplb {
namespace {

struct DummyClass {
  explicit DummyClass(int a) : a_(a) {}
  int a_;
};
typedef std::vector<DummyClass> DummyClasses;

// C++14 requires that the compiler can initialize std containers without
// braces.
void NoInitializationBraces() {
  const DummyClasses dummy_classes;
}

// Verify constexpr initialization. Supported from Clang 3.9, GCC 4.6, VS2017
// See: http://open-std.org/jtc1/sc22/wg21/docs/cwg_defects.html#253
struct CheckInitializerTag {};
static constexpr CheckInitializerTag kEmptyTag;

}  // namespace
}  // namespace nplb
}  // namespace starboard
