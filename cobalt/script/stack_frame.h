// Copyright 2015 Google Inc. All Rights Reserved.
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

#ifndef COBALT_SCRIPT_STACK_FRAME_H_
#define COBALT_SCRIPT_STACK_FRAME_H_

#include <string>
#include <vector>

namespace cobalt {
namespace script {

struct StackFrame {
  StackFrame() : line_number(0), column_number(0) {}
  StackFrame(int line_number, int column_number,
             const std::string& function_name, const std::string& source_url)
      : line_number(line_number),
        column_number(column_number),
        function_name(function_name),
        source_url(source_url) {}

  int line_number;
  int column_number;
  std::string function_name;
  std::string source_url;
};

// Convert a vector of stack frames to a string for output.
std::string StackTraceToString(const std::vector<StackFrame>& stack_frames);

}  // namespace script
}  // namespace cobalt

#endif  // COBALT_SCRIPT_STACK_FRAME_H_
