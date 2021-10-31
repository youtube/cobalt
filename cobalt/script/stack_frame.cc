// Copyright 2014 The Cobalt Authors. All Rights Reserved.
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

#include "cobalt/script/stack_frame.h"

#include <sstream>

namespace cobalt {
namespace script {

std::string StackTraceToString(const std::vector<StackFrame>& stack_frames) {
  std::stringstream backtrace_stream;
  for (size_t i = 0; i < stack_frames.size(); ++i) {
    if (i > 0) {
      backtrace_stream << std::endl;
    }
    backtrace_stream << stack_frames[i].function_name;
    if (!stack_frames[i].source_url.empty()) {
      backtrace_stream << " @ " << stack_frames[i].source_url << ':'
                       << stack_frames[i].line_number;
      backtrace_stream << ":" << stack_frames[i].column_number;
    }
  }
  return backtrace_stream.str();
}

}  // namespace script
}  // namespace cobalt
