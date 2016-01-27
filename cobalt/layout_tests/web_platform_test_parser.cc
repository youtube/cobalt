/*
 * Copyright 2015 Google Inc. All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#include "cobalt/layout_tests/web_platform_test_parser.h"

#include <set>
#include <utility>

#include "base/file_path.h"
#include "base/file_util.h"
#include "base/optional.h"
#include "base/string_number_conversions.h"
#include "base/string_util.h"
#include "cobalt/base/cobalt_paths.h"
#include "cobalt/layout_tests/test_utils.h"

namespace cobalt {
namespace layout_tests {

namespace {

std::string ExpectationToString(WebPlatformTestInfo::State state) {
  switch (state) {
    case WebPlatformTestInfo::kPass:
      return "PASS";
    case WebPlatformTestInfo::kFail:
      return "FAIL";
    case WebPlatformTestInfo::kDisable:
      return "DISABLE";
  }
  NOTREACHED();
  return "FAIL";
}

WebPlatformTestInfo::State StringToExpectation(
    const std::string& lower_case_string) {
  if (LowerCaseEqualsASCII(lower_case_string, "pass")) {
    return WebPlatformTestInfo::kPass;
  } else if (LowerCaseEqualsASCII(lower_case_string, "fail")) {
    return WebPlatformTestInfo::kFail;
  } else if (LowerCaseEqualsASCII(lower_case_string, "disable")) {
    return WebPlatformTestInfo::kDisable;
  } else {
    NOTREACHED() << "Invalid test expectation " << lower_case_string;
    return WebPlatformTestInfo::kFail;
  }
}

base::optional<WebPlatformTestInfo> ParseWebPlatformTestCaseLine(
    const std::string& line_string) {
  std::vector<std::string> test_case_tokens;
  Tokenize(line_string, ",", &test_case_tokens);
  if (test_case_tokens.empty() || test_case_tokens.size() > 2) {
    DLOG(WARNING) << "Failed to parse: " << line_string;
    return base::nullopt;
  }

  TrimWhitespaceASCII(test_case_tokens[0], TRIM_ALL, &test_case_tokens[0]);
  TrimWhitespaceASCII(test_case_tokens[1], TRIM_ALL, &test_case_tokens[1]);

  std::string test_expect = StringToLowerASCII(test_case_tokens[1]);
  WebPlatformTestInfo::State expectation = StringToExpectation(test_expect);
  if (expectation == WebPlatformTestInfo::kDisable) {
    return base::nullopt;
  } else {
    WebPlatformTestInfo test_info;
    test_info.url = test_case_tokens[0];
    test_info.expectation = expectation;
    return test_info;
  }
}

}  // namespace

std::ostream& operator<<(std::ostream& out,
                         const WebPlatformTestInfo& test_info) {
  return out << test_info.url
             << " Expected: " << ExpectationToString(test_info.expectation);
}

std::vector<WebPlatformTestInfo> EnumerateWebPlatformTests(
    const std::string& top_level) {
  FilePath test_dir(GetTestInputRootDirectory()
                        .Append("web-platform-tests")
                        .Append(top_level));
  FilePath tests_list_file(
      test_dir.Append(FILE_PATH_LITERAL("web_platform_tests.txt")));

  std::string test_list;
  if (!file_util::ReadFileToString(tests_list_file, &test_list)) {
    DLOG(ERROR) << "Could not open '" << tests_list_file.value() << "'.";
    return std::vector<WebPlatformTestInfo>();
  } else {
    // Tokenize the file contents into lines, and then read each line one by
    // one as the name of the test file.
    std::vector<std::string> line_tokens;
    Tokenize(test_list, "\n\r", &line_tokens);

    const char kCommentChar = '#';

    typedef std::set<WebPlatformTestInfo> TestInfoSet;
    TestInfoSet all_test_infos;
    for (std::vector<std::string>::const_iterator iter = line_tokens.begin();
         iter != line_tokens.end(); ++iter) {
      std::string trimmed_line;
      TrimWhitespaceASCII(*iter, TRIM_ALL, &trimmed_line);

      // Skip commented-out lines.
      if (trimmed_line.size() > 0 && trimmed_line[0] == kCommentChar) {
        continue;
      }

      base::optional<WebPlatformTestInfo> parsed_test_info =
          ParseWebPlatformTestCaseLine(trimmed_line);
      if (parsed_test_info) {
        WebPlatformTestInfo& test_info = *parsed_test_info;
        test_info.url = top_level + "/" + test_info.url;
        std::pair<TestInfoSet::iterator, bool> ret =
            all_test_infos.insert(test_info);
        if (ret.second == false) {
          NOTREACHED() << "Duplicate entry found: " << test_info;
        }
      }
    }

    std::vector<WebPlatformTestInfo> test_info_list(all_test_infos.begin(),
                                                    all_test_infos.end());
    return test_info_list;
  }
}

}  // namespace layout_tests
}  // namespace cobalt
