// Copyright 2023 The Cobalt Authors. All Rights Reserved.
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

#include <string>

namespace starboard {
namespace nplb {
namespace compiler_compliance {
namespace {

// Test string ends with support

void test_string_ends_with() {
  bool result = std::string("foobar").ends_with("bar");
}

}  // namespace
}  // namespace compiler_compliance
}  // namespace nplb
}  // namespace starboard
