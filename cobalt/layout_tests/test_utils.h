// Copyright 2015 The Cobalt Authors. All Rights Reserved.
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

#ifndef COBALT_LAYOUT_TESTS_TEST_UTILS_H_
#define COBALT_LAYOUT_TESTS_TEST_UTILS_H_

#include <string>

#include "base/containers/hash_tables.h"
#include "base/files/file_path.h"
#include "cobalt/base/log_message_handler.h"

namespace cobalt {
namespace layout_tests {

// Returns the root directory that all test input can be found in (e.g.
// the HTML files that define the tests, and the PNG/TXT files that define
// the expected output).
base::FilePath GetTestInputRootDirectory();

// Returns the root directory that all output will be placed within.  Output
// is generated when rebaselining test expected output, or when test details
// have been chosen to be output.
base::FilePath GetTestOutputRootDirectory();

class LogFilter {
 public:
  LogFilter();
  ~LogFilter();
  void Add(const std::string& s);

 private:
  bool OnLogMessage(int severity, const char* file, int line,
                    size_t message_start, const std::string& str);

  base::hash_set<std::string> filtered_strings_;
  base::LogMessageHandler::CallbackId callback_id_;

  DISALLOW_COPY_AND_ASSIGN(LogFilter);
};
}  // namespace layout_tests
}  // namespace cobalt

#endif  // COBALT_LAYOUT_TESTS_TEST_UTILS_H_
