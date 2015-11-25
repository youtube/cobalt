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

#ifndef LAYOUT_TESTS_WEB_PLATFORM_TEST_PARSER_H_
#define LAYOUT_TESTS_WEB_PLATFORM_TEST_PARSER_H_

#include <string>
#include <vector>

namespace cobalt {
namespace layout_tests {

// Final parsed information about an individual WebPlatform test entry.
struct WebPlatformTestInfo {
  enum ExpectedState {
    // Test should pass.
    kExpectedPass,
    // Test is currently known to fail due to a bug.
    kExpectedFail,
    // Feature under test is unsupported, so failure is expected.
    // No expectation this will ever be fixed, so don't log the JS errors.
    // But we shouldn't crash.
    kExpectedFailQuiet,
    // Test crashes or takes too long, etc., so don't run it at all.
    kExpectedDisable,
  };
  // URL of the web-platform-tests test case to run.
  std::string url;

  ExpectedState expectation;
};

// Define operator<< so that this test parameter can be printed by gtest if
// a test fails.
std::ostream& operator<<(std::ostream& out,
                         const WebPlatformTestInfo& test_info);

// Similar to EnumerateLayoutTests(), but reads web_platform_tests.txt
std::vector<WebPlatformTestInfo> EnumerateWebPlatformTests(
    const std::string& top_level);

}  // namespace layout_tests
}  // namespace cobalt

#endif  // LAYOUT_TESTS_WEB_PLATFORM_TEST_PARSER_H_
