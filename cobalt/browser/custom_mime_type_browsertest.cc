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

#include <string>
#include <vector>

#include "cobalt/testing/browser_tests/browser/test_shell.h"
#include "cobalt/testing/browser_tests/content_browser_test.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "starboard/media.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace cobalt {
namespace {

// Global storage to capture intercepted calls across the single-process
// boundary.
std::vector<std::string>* g_intercepted_mimes = nullptr;
std::vector<std::string>* g_intercepted_key_systems = nullptr;
SbMediaSupportType g_mock_support_type = kSbMediaSupportTypeNotSupported;

SbMediaSupportType InterceptCanPlay(const char* mime, const char* key_system) {
  if (g_intercepted_mimes && mime) {
    g_intercepted_mimes->push_back(mime);
  }
  if (g_intercepted_key_systems && key_system) {
    g_intercepted_key_systems->push_back(key_system);
  }
  return g_mock_support_type;
}

}  // namespace

class CustomMimeTypeBrowserTest : public content::ContentBrowserTest {
 public:
  CustomMimeTypeBrowserTest() = default;
  ~CustomMimeTypeBrowserTest() override = default;

  void SetUpOnMainThread() override {
    content::ContentBrowserTest::SetUpOnMainThread();
    g_intercepted_mimes = &intercepted_mimes_;
    g_intercepted_key_systems = &intercepted_key_systems_;
    SbMediaSetCanPlayMimeAndKeySystemFuncForTesting(&InterceptCanPlay);
  }

  void TearDownOnMainThread() override {
    SbMediaSetCanPlayMimeAndKeySystemFuncForTesting(nullptr);
    g_intercepted_mimes = nullptr;
    g_intercepted_key_systems = nullptr;
    content::ContentBrowserTest::TearDownOnMainThread();
  }

 protected:
  std::vector<std::string> intercepted_mimes_;
  std::vector<std::string> intercepted_key_systems_;
};

IN_PROC_BROWSER_TEST_F(CustomMimeTypeBrowserTest,
                       IsTypeSupported_ForwardsRawCustomAttributes) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url = embedded_test_server()->GetURL("/title1.html");
  ASSERT_TRUE(NavigateToURL(shell()->web_contents(), url));

  intercepted_mimes_.clear();
  g_mock_support_type = kSbMediaSupportTypeProbably;

  const char kCustomMime[] =
      "video/mp4; codecs=\"avc1.64002a\"; width=3840; height=2160; "
      "tunnelmode=true; hdr=hdr10plus";

  std::string js_query =
      std::string("MediaSource.isTypeSupported('") + kCustomMime + "');";
  EXPECT_TRUE(content::EvalJs(shell()->web_contents(), js_query).ExtractBool());

  ASSERT_FALSE(intercepted_mimes_.empty());
  EXPECT_EQ(intercepted_mimes_.back(), kCustomMime);
}

IN_PROC_BROWSER_TEST_F(CustomMimeTypeBrowserTest,
                       CanPlayType_ForwardsRawCustomAttributes) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url = embedded_test_server()->GetURL("/title1.html");
  ASSERT_TRUE(NavigateToURL(shell()->web_contents(), url));

  intercepted_mimes_.clear();
  g_mock_support_type = kSbMediaSupportTypeMaybe;

  const char kCustomMime[] =
      "video/mp4; codecs=\"avc1.64002a\"; width=1920; height=1080; "
      "tunnelmode=true;";

  std::string js_query =
      std::string("document.createElement('video').canPlayType('") +
      kCustomMime + "');";
  EXPECT_EQ("maybe",
            content::EvalJs(shell()->web_contents(), js_query).ExtractString());

  ASSERT_FALSE(intercepted_mimes_.empty());
  EXPECT_EQ(intercepted_mimes_.back(), kCustomMime);
}

IN_PROC_BROWSER_TEST_F(CustomMimeTypeBrowserTest,
                       IsTypeSupported_NotSupportedReturnsFalse) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url = embedded_test_server()->GetURL("/title1.html");
  ASSERT_TRUE(NavigateToURL(shell()->web_contents(), url));

  intercepted_mimes_.clear();
  g_mock_support_type = kSbMediaSupportTypeNotSupported;

  const char kUnsupportedMime[] =
      "video/mp4; codecs=\"unsupported.codec\"; width=99999; height=99999;";

  std::string js_query =
      std::string("MediaSource.isTypeSupported('") + kUnsupportedMime + "');";
  EXPECT_FALSE(
      content::EvalJs(shell()->web_contents(), js_query).ExtractBool());

  ASSERT_FALSE(intercepted_mimes_.empty());
  EXPECT_EQ(intercepted_mimes_.back(), kUnsupportedMime);
}

IN_PROC_BROWSER_TEST_F(CustomMimeTypeBrowserTest,
                       IsTypeSupported_ForwardsHdr10PlusAttribute) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url = embedded_test_server()->GetURL("/title1.html");
  ASSERT_TRUE(NavigateToURL(shell()->web_contents(), url));

  intercepted_mimes_.clear();
  g_mock_support_type = kSbMediaSupportTypeProbably;

  const char kHdr10PlusMime[] =
      "video/webm; codecs=\"vp09.02.51.10.01.09.16.09.00\"; hdr=hdr10plus";

  std::string js_query =
      std::string("MediaSource.isTypeSupported('") + kHdr10PlusMime + "');";
  EXPECT_TRUE(content::EvalJs(shell()->web_contents(), js_query).ExtractBool());

  ASSERT_FALSE(intercepted_mimes_.empty());
  EXPECT_EQ(intercepted_mimes_.back(), kHdr10PlusMime);
}

IN_PROC_BROWSER_TEST_F(CustomMimeTypeBrowserTest,
                       CanPlayType_ForwardsHdr10PlusAttribute) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url = embedded_test_server()->GetURL("/title1.html");
  ASSERT_TRUE(NavigateToURL(shell()->web_contents(), url));

  intercepted_mimes_.clear();
  g_mock_support_type = kSbMediaSupportTypeProbably;

  const char kHdr10PlusMime[] =
      "video/mp4; codecs=\"hvc1.2.4.L153.B0\"; hdr=hdr10plus";

  std::string js_query =
      std::string("document.createElement('video').canPlayType('") +
      kHdr10PlusMime + "');";
  EXPECT_EQ("probably",
            content::EvalJs(shell()->web_contents(), js_query).ExtractString());

  ASSERT_FALSE(intercepted_mimes_.empty());
  EXPECT_EQ(intercepted_mimes_.back(), kHdr10PlusMime);
}

}  // namespace cobalt
