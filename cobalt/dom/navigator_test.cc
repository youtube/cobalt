// Copyright 2022 The Cobalt Authors. All Rights Reserved.
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

#include <memory>
#include <string>

#include "base/logging.h"
#include "cobalt/bindings/testing/utils.h"
#include "cobalt/dom/testing/test_with_javascript.h"
#include "cobalt/web/testing/gtest_workarounds.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::SaveArg;

namespace cobalt {
namespace dom {

namespace {
class NavigatorTest : public testing::TestWithJavaScript {};
}  // namespace


TEST_F(NavigatorTest, Navigator) {
  std::string result;
  EXPECT_TRUE(EvaluateScript("typeof navigator", &result));
  EXPECT_EQ("object", result);

  EXPECT_TRUE(EvaluateScript("navigator", &result));
  EXPECT_TRUE(
      bindings::testing::IsAcceptablePrototypeString("Navigator", result));
}

TEST_F(NavigatorTest, WindowNavigator) {
  std::string result;
  EXPECT_TRUE(EvaluateScript("typeof window.navigator", &result));
  EXPECT_EQ("object", result);

  EXPECT_TRUE(EvaluateScript("window.navigator", &result));
  EXPECT_TRUE(
      bindings::testing::IsAcceptablePrototypeString("Navigator", result));
}

TEST_F(NavigatorTest, NavigatorID) {
  std::string result;
  EXPECT_TRUE(EvaluateScript("typeof navigator.userAgent", &result));
  EXPECT_EQ("string", result);

  EXPECT_TRUE(EvaluateScript("navigator.userAgent", &result));
  EXPECT_EQ("StubUserAgentString", result);
}

TEST_F(NavigatorTest, NavigatorLanguage) {
  std::string result;
  EXPECT_TRUE(EvaluateScript("typeof navigator.language", &result));
  EXPECT_EQ("string", result);
  EXPECT_TRUE(EvaluateScript("navigator.language", &result));
  EXPECT_EQ("StubPreferredLanguageString", result);

  EXPECT_TRUE(EvaluateScript("typeof navigator.languages", &result));
  EXPECT_EQ("object", result);
  EXPECT_TRUE(EvaluateScript("navigator.languages", &result));
  EXPECT_EQ("StubPreferredLanguageString", result);

  EXPECT_TRUE(EvaluateScript("typeof navigator.languages.length", &result));
  EXPECT_EQ("number", result);
  EXPECT_TRUE(EvaluateScript("navigator.languages.length", &result));
  EXPECT_EQ("1", result);
  EXPECT_TRUE(EvaluateScript("navigator.languages[0]", &result));
  EXPECT_EQ("StubPreferredLanguageString", result);
}

TEST_F(NavigatorTest, NavigatorPluginsPlugins) {
  std::string result;
  EXPECT_TRUE(EvaluateScript("typeof navigator.plugins", &result));
  EXPECT_EQ("object", result);
  EXPECT_TRUE(EvaluateScript("navigator.plugins", &result));
  EXPECT_TRUE(
      bindings::testing::IsAcceptablePrototypeString("PluginArray", result));
  EXPECT_TRUE(EvaluateScript("typeof navigator.plugins.length", &result));
  EXPECT_EQ("number", result);
  EXPECT_TRUE(EvaluateScript("navigator.plugins.length", &result));
  EXPECT_EQ("0", result);
}

TEST_F(NavigatorTest, NavigatorPluginsMimeTypes) {
  std::string result;
  EXPECT_TRUE(EvaluateScript("typeof navigator.mimeTypes", &result));
  EXPECT_EQ("object", result);
  EXPECT_TRUE(EvaluateScript("navigator.mimeTypes", &result));
  EXPECT_TRUE(
      bindings::testing::IsAcceptablePrototypeString("MimeTypeArray", result));
  EXPECT_TRUE(EvaluateScript("typeof navigator.mimeTypes.length", &result));
  EXPECT_EQ("number", result);
  EXPECT_TRUE(EvaluateScript("navigator.mimeTypes.length", &result));
  EXPECT_EQ("0", result);
}

TEST_F(NavigatorTest, NavigatorPluginsJavaEnabled) {
  std::string result;
  EXPECT_TRUE(EvaluateScript("typeof navigator.javaEnabled", &result));
  EXPECT_EQ("boolean", result);
  EXPECT_TRUE(EvaluateScript("navigator.javaEnabled", &result));
  EXPECT_EQ("false", result);
}

TEST_F(NavigatorTest, NavigatorStorageUtils) {
  std::string result;
  EXPECT_TRUE(EvaluateScript("typeof navigator.cookieEnabled", &result));
  EXPECT_EQ("boolean", result);
  EXPECT_TRUE(EvaluateScript("navigator.cookieEnabled", &result));
  EXPECT_EQ("false", result);
}

TEST_F(NavigatorTest, NavigatorMediaSession) {
  std::string result;
  EXPECT_TRUE(EvaluateScript("typeof navigator.mediaSession", &result));
  EXPECT_EQ("object", result);
  EXPECT_TRUE(EvaluateScript("navigator.mediaSession", &result));
  EXPECT_TRUE(
      bindings::testing::IsAcceptablePrototypeString("MediaSession", result));
}

TEST_F(NavigatorTest, NavigatorSystemCaptionSettings) {
  std::string result;
  EXPECT_TRUE(
      EvaluateScript("typeof navigator.systemCaptionSettings", &result));
  EXPECT_EQ("object", result);
  EXPECT_TRUE(EvaluateScript("navigator.systemCaptionSettings", &result));
  EXPECT_TRUE(bindings::testing::IsAcceptablePrototypeString(
      "SystemCaptionSettings", result));
}

TEST_F(NavigatorTest, NavigatorLicenses) {
  std::string result;
  EXPECT_TRUE(EvaluateScript("typeof navigator.licenses", &result));
  EXPECT_EQ("string", result);
  EXPECT_TRUE(EvaluateScript("navigator.licenses", &result));
  // Expect the license string to be at least 32KB.
  EXPECT_GT(result.length(), 32768UL);
}

TEST_F(NavigatorTest, NavigatorOnline) {
  std::string result;
  EXPECT_TRUE(EvaluateScript("typeof navigator.onLine", &result));
  EXPECT_EQ("boolean", result);
  EXPECT_TRUE(EvaluateScript("navigator.onLine", &result));
  EXPECT_EQ("true", result);
}

}  // namespace dom
}  // namespace cobalt
