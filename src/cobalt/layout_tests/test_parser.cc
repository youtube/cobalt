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

#include "cobalt/layout_tests/test_parser.h"

#include "base/file_path.h"
#include "base/file_util.h"
#include "base/optional.h"
#include "base/path_service.h"
#include "base/string_number_conversions.h"
#include "base/string_util.h"
#include "cobalt/base/cobalt_paths.h"

namespace cobalt {
namespace layout_tests {

std::ostream& operator<<(std::ostream& out, const TestInfo& test_info) {
  return out << test_info.base_file_path.value();
}

namespace {

// Returns the relative path to Cobalt layout tests.  This can be appended
// to either base::DIR_SOURCE_ROOT to get the input directory or
// base::DIR_COBALT_TEST_OUT to get the output directory.
FilePath GetDirSourceRootRelativePath() {
  return FilePath(FILE_PATH_LITERAL("cobalt"))
      .Append(FILE_PATH_LITERAL("layout_tests"));
}

GURL GetURLFromBaseFilePath(const FilePath& base_file_path) {
  return GURL("file:///" + GetDirSourceRootRelativePath().value() + "/" +
              base_file_path.AddExtension("html").value());
}

}  // namespace

namespace {

base::optional<TestInfo> ParseLayoutTestCaseLine(
    const FilePath& top_level, const std::string& line_string) {
  // Split the line up by commas, of which there may be none.
  std::vector<std::string> test_case_tokens;
  Tokenize(line_string, ",", &test_case_tokens);
  if (test_case_tokens.empty() || test_case_tokens.size() > 2) {
    return base::nullopt;
  }

  // Extract the test case file path as the first element before a comma, if
  // there is one. The test case file path can optionally be postfixed by a
  // colon and a viewport resolution.
  std::vector<std::string> file_path_tokens;
  Tokenize(test_case_tokens[0], ":", &file_path_tokens);
  DCHECK(!file_path_tokens.empty());

  base::optional<math::Size> viewport_size;
  if (file_path_tokens.size() > 1) {
    DCHECK_EQ(2, file_path_tokens.size());
    // If there is a colon, the string that comes after the colon contains the
    // explicitly specified resolution in pixels, formatted as 'width x height'.
    std::vector<std::string> resolution_tokens;
    Tokenize(file_path_tokens[1], "x", &resolution_tokens);
    if (resolution_tokens.size() == 2) {
      int width, height;
      base::StringToInt(resolution_tokens[0], &width);
      base::StringToInt(resolution_tokens[1], &height);
      viewport_size.emplace(width, height);
    }
  }

  std::string base_file_path_string;
  TrimWhitespaceASCII(file_path_tokens[0], TRIM_ALL, &base_file_path_string);
  if (base_file_path_string.empty()) {
    return base::nullopt;
  }
  FilePath base_file_path(top_level.Append(base_file_path_string));

  if (test_case_tokens.size() == 1) {
    // If there is no comma, determine the URL from the file path.
    return TestInfo(base_file_path, GetURLFromBaseFilePath(base_file_path),
                    viewport_size);
  } else {
    // If there is a comma, the string that comes after it contains the
    // explicitly specified URL.  Extract that and use it as the test URL.
    DCHECK_EQ(2, test_case_tokens.size());
    std::string url_string;
    TrimWhitespaceASCII(test_case_tokens[1], TRIM_ALL, &url_string);
    if (url_string.empty()) {
      return base::nullopt;
    }
    return TestInfo(base_file_path, GURL(url_string), viewport_size);
  }
}

}  // namespace

std::vector<TestInfo> EnumerateLayoutTests(const std::string& top_level) {
  FilePath test_dir(GetTestInputRootDirectory().Append(top_level));
  FilePath layout_tests_list_file(
      test_dir.Append(FILE_PATH_LITERAL("layout_tests.txt")));

  std::string layout_test_list_string;
  if (!file_util::ReadFileToString(layout_tests_list_file,
                                   &layout_test_list_string)) {
    DLOG(ERROR) << "Could not open '" << layout_tests_list_file.value() << "'.";
    return std::vector<TestInfo>();
  } else {
    // Tokenize the file contents into lines, and then read each line one by
    // one as the name of the test file.
    std::vector<std::string> line_tokens;
    Tokenize(layout_test_list_string, "\n\r", &line_tokens);

    std::vector<TestInfo> test_info_list;
    for (std::vector<std::string>::const_iterator iter = line_tokens.begin();
         iter != line_tokens.end(); ++iter) {
      base::optional<TestInfo> parsed_test_info =
          ParseLayoutTestCaseLine(FilePath(top_level), *iter);
      if (!parsed_test_info) {
        DLOG(WARNING) << "Ignoring invalid test case line: " << iter->c_str();
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
