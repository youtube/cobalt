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

#include "cobalt/layout_tests/test_parser.h"

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/optional.h"
#include "base/path_service.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "cobalt/base/cobalt_paths.h"
#include "cobalt/layout_tests/test_utils.h"

using cobalt::cssom::ViewportSize;

namespace cobalt {
namespace layout_tests {

namespace {

// Returns the relative path to Cobalt layout tests.  This can be appended
// to either base::DIR_TEST_DATA to get the input directory or
// base::DIR_COBALT_TEST_OUT to get the output directory.
base::FilePath GetTestDirRelativePath() {
  return base::FilePath(FILE_PATH_LITERAL("cobalt"))
      .Append(FILE_PATH_LITERAL("layout_tests"));
}

GURL GetURLFromBaseFilePath(const base::FilePath& base_file_path) {
  return GURL("file:///" + GetTestDirRelativePath().value() + "/" +
              base_file_path.AddExtension("html").value());
}

base::Optional<TestInfo> ParseLayoutTestCaseLine(
    const base::FilePath& top_level, const std::string& line_string) {
  // The test case file path can optionally be postfixed by a colon and a
  // viewport resolution.
  std::vector<std::string> file_path_tokens = base::SplitString(
      line_string, ":", base::KEEP_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
  DCHECK(!file_path_tokens.empty());

  base::Optional<cssom::ViewportSize> viewport_size;
  if (file_path_tokens.size() > 1) {
    DCHECK_EQ(2u, file_path_tokens.size());
    // If there is a colon, the string that comes after the colon contains the
    // explicitly specified resolution in pixels, formatted as 'width x height'.
    std::vector<std::string> resolution_tokens =
        base::SplitString(file_path_tokens[1], "x", base::KEEP_WHITESPACE,
                          base::SPLIT_WANT_NONEMPTY);
    if (resolution_tokens.size() == 2) {
      int width, height;
      base::StringToInt(resolution_tokens[0], &width);
      base::StringToInt(resolution_tokens[1], &height);
      viewport_size.emplace(width, height);
    }
  }

  std::string base_file_path_string;
  TrimWhitespaceASCII(file_path_tokens[0], base::TRIM_ALL,
                      &base_file_path_string);
  if (base_file_path_string.empty()) {
    return base::nullopt;
  }
  base::FilePath base_file_path(top_level.Append(base_file_path_string));

  return TestInfo(base_file_path, GetURLFromBaseFilePath(base_file_path),
                  viewport_size);
}

}  // namespace

std::ostream& operator<<(std::ostream& out, const TestInfo& test_info) {
  return out << test_info.base_file_path.value();
}

std::vector<TestInfo> EnumerateLayoutTests(const std::string& top_level) {
  base::FilePath test_dir(GetTestInputRootDirectory().Append(top_level));
  base::FilePath layout_tests_list_file(
      test_dir.Append(FILE_PATH_LITERAL("layout_tests.txt")));

  std::string layout_test_list_string;
  if (!base::ReadFileToString(layout_tests_list_file,
                              &layout_test_list_string)) {
    return std::vector<TestInfo>();
  } else {
    // base::SplitString the file contents into lines, and then read each line
    // one by one as the name of the test file.
    std::vector<std::string> line_tokens =
        base::SplitString(layout_test_list_string, "\n\r",
                          base::KEEP_WHITESPACE, base::SPLIT_WANT_NONEMPTY);

    std::vector<TestInfo> test_info_list;
    for (std::vector<std::string>::const_iterator iter = line_tokens.begin();
         iter != line_tokens.end(); ++iter) {
      const char kCommentChar = '#';
      if (!iter->empty() && iter->front() == kCommentChar) {
        // Skip comment lines.
        continue;
      }
      base::Optional<TestInfo> parsed_test_info =
          ParseLayoutTestCaseLine(base::FilePath(top_level), *iter);
      if (!parsed_test_info) {
        continue;
      } else {
        test_info_list.push_back(*parsed_test_info);
      }
    }
    return test_info_list;
  }
}

}  // namespace layout_tests
}  // namespace cobalt
