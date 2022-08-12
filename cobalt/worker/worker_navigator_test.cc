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
#include "cobalt/web/testing/gtest_workarounds.h"
#include "cobalt/web/url.h"
#include "cobalt/worker/testing/test_with_javascript.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::SaveArg;

namespace cobalt {
namespace worker {

namespace {
class WorkerNavigatorTest : public testing::TestWorkersWithJavaScript {};
}  // namespace


TEST_P(WorkerNavigatorTest, WorkerNavigator) {
  std::string result;
  EXPECT_TRUE(EvaluateScript("typeof navigator", &result));
  EXPECT_EQ("object", result);

  EXPECT_TRUE(EvaluateScript("navigator", &result));
  EXPECT_TRUE(bindings::testing::IsAcceptablePrototypeString("WorkerNavigator",
                                                             result));
}

TEST_P(WorkerNavigatorTest, ThisWorkerNavigator) {
  std::string result;
  EXPECT_TRUE(EvaluateScript("typeof this.navigator", &result));
  EXPECT_EQ("object", result);

  EXPECT_TRUE(EvaluateScript("this.navigator", &result));
  EXPECT_TRUE(bindings::testing::IsAcceptablePrototypeString("WorkerNavigator",
                                                             result));
}

TEST_P(WorkerNavigatorTest, WorkerNavigatorID) {
  std::string result;
  EXPECT_TRUE(EvaluateScript("typeof navigator.userAgent", &result));
  EXPECT_EQ("string", result);

  EXPECT_TRUE(EvaluateScript("navigator.userAgent", &result));
  EXPECT_EQ("StubUserAgentString", result);
}

TEST_P(WorkerNavigatorTest, WorkerNavigatorLanguage) {
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

TEST_P(WorkerNavigatorTest, WorkerNavigatorOnline) {
  std::string result;
  EXPECT_TRUE(EvaluateScript("typeof navigator.onLine", &result));
  EXPECT_EQ("boolean", result);
  EXPECT_TRUE(EvaluateScript("navigator.onLine", &result));
  EXPECT_EQ("true", result);
}

INSTANTIATE_TEST_CASE_P(
    WorkerNavigatorTests, WorkerNavigatorTest,
    ::testing::ValuesIn(testing::TestWorkersWithJavaScript::GetWorkerTypes()),
    testing::TestWorkersWithJavaScript::GetTypeName);

}  // namespace worker
}  // namespace cobalt
