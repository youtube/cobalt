// Copyright 2026 The Cobalt Authors. All Rights Reserved.
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

#include "build/build_config.h"
#include "cobalt/testing/browser_tests/browser/test_shell.h"
#include "cobalt/testing/browser_tests/content_browser_test.h"
#include "cobalt/testing/browser_tests/content_browser_test_utils.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace cobalt {

#if (BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_ANDROID)) && !BUILDFLAG(IS_STARBOARD)
class PerformanceBrowserTest : public content::ContentBrowserTest {
 public:
  PerformanceBrowserTest() = default;
  ~PerformanceBrowserTest() override = default;
};

IN_PROC_BROWSER_TEST_F(PerformanceBrowserTest, VerifyDecodedImagesMemoryApis) {
  ASSERT_TRUE(content::NavigateToURL(
      shell(), content::GetTestUrl(nullptr, "title1.html")));

  // Verify that window.performance exists.
  EXPECT_TRUE(content::EvalJs(shell()->web_contents(),
                              "typeof window.performance !== 'undefined'")
                  .ExtractBool());

  // Verify measureDecodedImagesMemory exists and returns a non-negative
  // integer.
  EXPECT_TRUE(
      content::EvalJs(
          shell()->web_contents(),
          "typeof window.performance.measureDecodedImagesMemory === 'function'")
          .ExtractBool());
  EXPECT_GE(content::EvalJs(shell()->web_contents(),
                            "window.performance.measureDecodedImagesMemory()")
                .ExtractInt(),
            0);

  // Verify measureDecodedImagesPeakMemory exists and returns a non-negative
  // integer.
  EXPECT_TRUE(
      content::EvalJs(
          shell()->web_contents(),
          "typeof window.performance.measureDecodedImagesPeakMemory === "
          "'function'")
          .ExtractBool());
  EXPECT_GE(
      content::EvalJs(shell()->web_contents(),
                      "window.performance.measureDecodedImagesPeakMemory()")
          .ExtractInt(),
      0);

  // Verify measureSystemMemoryInfo includes decodedImageCacheMemory and
  // decodedImageCachePeakMemory.
  EXPECT_TRUE(
      content::EvalJs(
          shell()->web_contents(),
          "typeof window.performance.measureSystemMemoryInfo === 'function'")
          .ExtractBool());
  EXPECT_TRUE(
      content::EvalJs(
          shell()->web_contents(),
          "(() => {\n"
          "  const info = window.performance.measureSystemMemoryInfo();\n"
          "  return typeof info === 'object' &&\n"
          "         info !== null &&\n"
          "         'decodedImageCacheMemory' in info &&\n"
          "         typeof info.decodedImageCacheMemory === 'number' &&\n"
          "         info.decodedImageCacheMemory >= 0 &&\n"
          "         'decodedImageCachePeakMemory' in info &&\n"
          "         typeof info.decodedImageCachePeakMemory === 'number' &&\n"
          "         info.decodedImageCachePeakMemory >= 0;\n"
          "})()")
          .ExtractBool());
}
#endif  // (BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_ANDROID)) &&
        // !BUILDFLAG(IS_STARBOARD)

}  // namespace cobalt
