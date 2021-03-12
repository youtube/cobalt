// Copyright 2016 The Cobalt Authors. All Rights Reserved.
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

#include "starboard/nplb/blitter_pixel_tests/command_line.h"

namespace starboard {
namespace nplb {
namespace blitter_pixel_tests {

namespace {
// Global variables used to keep track of the command line arguments so that
// they can be accessed later.
int g_argument_count = 0;
char** g_argument_values = NULL;
}  // namespace

void CommandLineSet(int argument_count, char** argument_values) {
  g_argument_count = argument_count;
  g_argument_values = argument_values;
}

bool CommandLineContains(const std::string& value) {
  for (int i = 0; i < g_argument_count; ++i) {
    if (std::string(g_argument_values[i]) == value) {
      return true;
    }
  }

  return false;
}

}  // namespace blitter_pixel_tests
}  // namespace nplb
}  // namespace starboard
