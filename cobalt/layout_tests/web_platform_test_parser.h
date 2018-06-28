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

#ifndef COBALT_LAYOUT_TESTS_WEB_PLATFORM_TEST_PARSER_H_
#define COBALT_LAYOUT_TESTS_WEB_PLATFORM_TEST_PARSER_H_

#include <set>
#include <string>
#include <vector>

namespace cobalt {
namespace layout_tests {

// Final parsed information about an individual WebPlatform test entry.
struct WebPlatformTestInfo {
  enum State {
    // Test should pass.
    kPass,
    // Test is expected to fail.
    kFail,
    // Test crashes or takes too long, etc., so don't run it at all.
    kDisable,
  };
  // URL of the web-platform-tests test case to run.
  std::string url;

  State expectation;

  // Only relevant when |expectation| is kPass or kFail. Tests with these names
  // are expected to fail or pass if |expectation| is kPass or kFail
  // respectively.
  std::set<std::string> exceptions;

  bool operator<(const WebPlatformTestInfo& rhs) const { return url < rhs.url; }
};

// Define operator<< so that this test parameter can be printed by gtest if
// a test fails.
std::ostream& operator<<(std::ostream& out,
                         const WebPlatformTestInfo& test_info);

// Similar to EnumerateLayoutTests(), but reads web_platform_tests.txt
// |precondition| may optionally be supplied. It represents a javascript
//   expression which must return "true" for tests to be enumerated.
std::vector<WebPlatformTestInfo> EnumerateWebPlatformTests(
    const std::string& top_level, const char* precondition = NULL);

}  // namespace layout_tests
}  // namespace cobalt

#endif  // COBALT_LAYOUT_TESTS_WEB_PLATFORM_TEST_PARSER_H_
