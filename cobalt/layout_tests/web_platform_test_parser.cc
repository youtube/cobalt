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
#include "cobalt/layout_tests/web_platform_test_parser.h"

#include <map>
#include <utility>

#include "base/file_path.h"
#include "base/file_util.h"
#include "base/optional.h"
#include "base/string_number_conversions.h"
#include "base/string_util.h"
#include "cobalt/base/cobalt_paths.h"
#include "cobalt/layout_tests/test_utils.h"
#include "cobalt/script/global_environment.h"
#include "cobalt/script/javascript_engine.h"
#include "cobalt/script/source_code.h"

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
  if (test_case_tokens.size() < 2) {
    DLOG(WARNING) << "Failed to parse: " << line_string;
    return base::nullopt;
  }

  for (size_t i = 0; i < test_case_tokens.size(); ++i) {
    TrimWhitespaceASCII(test_case_tokens[i], TRIM_ALL, &test_case_tokens[i]);
  }

  std::string test_expect = StringToLowerASCII(test_case_tokens[1]);
  WebPlatformTestInfo::State expectation = StringToExpectation(test_expect);
  if (expectation == WebPlatformTestInfo::kDisable) {
    return base::nullopt;
  } else {
    WebPlatformTestInfo test_info;
    test_info.url = test_case_tokens[0];
    test_info.expectation = expectation;
    for (size_t i = 2; i < test_case_tokens.size(); ++i) {
      test_info.exceptions.insert(test_case_tokens[i]);
    }
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
    const std::string& top_level, const char* precondition) {
  if (precondition) {
    // Evaluate the javascript precondition. Enumerate the web platform tests
    // only if the precondition is true.
    scoped_ptr<script::JavaScriptEngine> engine =
        script::JavaScriptEngine::CreateEngine();
    scoped_refptr<script::GlobalEnvironment> global_environment =
        engine->CreateGlobalEnvironment();
    global_environment->CreateGlobalObject();

    std::string result;
    bool success = global_environment->EvaluateScript(
        script::SourceCode::CreateSourceCode(
            precondition, base::SourceLocation(__FILE__, __LINE__, 1)),
        &result);

    if (!success) {
      DLOG(ERROR) << "Failed to evaluate precondition: "
                  << "\"" << precondition << "\"";
      // Continue to enumerate tests like normal.
    } else if (result != "true") {
      DLOG(WARNING) << "Skipping Web Platform Tests for "
                    << "\"" << top_level << "\"";
      return std::vector<WebPlatformTestInfo>();
    }
  }

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

    typedef std::map<std::string, WebPlatformTestInfo> TestInfoMap;
    TestInfoMap all_test_infos;
    for (std::vector<std::string>::iterator iter = line_tokens.begin();
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
        std::pair<TestInfoMap::iterator, bool> ret =
            all_test_infos.insert(std::make_pair(test_info.url, test_info));
        // If it's the same url, merge the exceptions into the original one.
        if (ret.second == false) {
          // Ensure that neither expectation is set to disable, and that the
          // expectations are different.
          DCHECK_NE(ret.first->second.expectation,
                    WebPlatformTestInfo::kDisable);
          DCHECK_NE(test_info.expectation, WebPlatformTestInfo::kDisable);
          DCHECK_NE(ret.first->second.expectation, test_info.expectation);

          // There should be at least one test in the list of exceptions for
          // the new one. Append these to the existing exceptions list.
          DCHECK_GT(test_info.exceptions.size(), size_t(0));
          ret.first->second.exceptions.insert(test_info.exceptions.begin(),
                                              test_info.exceptions.end());
        }
      }
    }

    std::vector<WebPlatformTestInfo> test_info_list;
    for (TestInfoMap::iterator it = all_test_infos.begin();
         it != all_test_infos.end(); ++it) {
      test_info_list.push_back(it->second);
    }
    return test_info_list;
  }
}

}  // namespace layout_tests
}  // namespace cobalt
