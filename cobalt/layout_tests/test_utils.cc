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

#include "cobalt/layout_tests/test_utils.h"

#include "base/bind.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/string_util.h"
#include "cobalt/base/cobalt_paths.h"
#include "googleurl/src/gurl.h"

namespace cobalt {
namespace layout_tests {

namespace {

// Returns the relative path to Cobalt layout tests.  This can be appended
// to either base::DIR_TEST_DATA to get the input directory or
// base::DIR_COBALT_TEST_OUT to get the output directory.
FilePath GetTestDirRelativePath() {
  return FilePath(FILE_PATH_LITERAL("cobalt"))
      .Append(FILE_PATH_LITERAL("layout_tests"));
}

}  // namespace

FilePath GetTestInputRootDirectory() {
  FilePath dir_test_data;
  CHECK(PathService::Get(base::DIR_TEST_DATA, &dir_test_data));
  return dir_test_data.Append(GetTestDirRelativePath());
}

FilePath GetTestOutputRootDirectory() {
  FilePath dir_cobalt_test_out;
  PathService::Get(cobalt::paths::DIR_COBALT_TEST_OUT, &dir_cobalt_test_out);
  return dir_cobalt_test_out.Append(GetTestDirRelativePath());
}

LogFilter::LogFilter() {
  callback_id_ = base::LogMessageHandler::GetInstance()->AddCallback(
      base::Bind(&LogFilter::OnLogMessage, base::Unretained(this)));
}

LogFilter::~LogFilter() {
  base::LogMessageHandler::GetInstance()->RemoveCallback(callback_id_);
}

void LogFilter::Add(const std::string& s) { filtered_strings_.insert(s); }

bool LogFilter::OnLogMessage(int /*severity*/, const char* /*file*/,
                             int /*line*/, size_t message_start,
                             const std::string& str) {
  std::string trimmed_message = str.substr(message_start);
  TrimWhitespaceASCII(trimmed_message, TRIM_ALL, &trimmed_message);
  if (filtered_strings_.find(trimmed_message) != filtered_strings_.end()) {
    return true;
  } else {
    return false;
  }
}

}  // namespace layout_tests
}  // namespace cobalt
