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

#ifndef STARBOARD_NPLB_BLITTER_PIXEL_TESTS_COMMAND_LINE_H_
#define STARBOARD_NPLB_BLITTER_PIXEL_TESTS_COMMAND_LINE_H_

#include <string>

namespace starboard {
namespace nplb {
namespace blitter_pixel_tests {

// Helper functions for allowing NPLB tests access to the command line options
// used to start the tests.

// CommandLineSet() must be called to set the argument values.  This should
// be called once on startup.
void CommandLineSet(int argument_count, char** argument_values);

// This command can be called to check if |arg| exists within any of the passed
// in command line arguments.
bool CommandLineContains(const std::string& arg);

}  // namespace blitter_pixel_tests
}  // namespace nplb
}  // namespace starboard

#endif  // STARBOARD_NPLB_BLITTER_PIXEL_TESTS_COMMAND_LINE_H_
