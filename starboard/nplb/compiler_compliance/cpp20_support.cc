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

#include <utility>

#include "starboard/configuration.h"
#include "starboard/types.h"

namespace starboard {
namespace nplb {
namespace {

struct Point {
  float x = 0.0;
  float y = 0.0;
  float z = 0.0;
};

void test_designated_initializer() {
  Point p = {
      .x = 1.0, .y = 2.0,
      // z will be 0.0
  };
}

}  // namespace
}  // namespace nplb
}  // namespace starboard
