// Copyright 2023 The Cobalt Authors. All Rights Reserved.
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

#include "cobalt/web/message_channel.h"

#include <string>

#include "base/strings/string_util.h"
#include "cobalt/web/testing/test_with_javascript.h"


#define EXPECT_SUBSTRING(needle, haystack) \
  EXPECT_PRED_FORMAT2(::testing::IsSubstring, (needle), (haystack))

namespace cobalt {
namespace web {

namespace {
class MessageChannelTestWithJavaScript : public testing::TestWebWithJavaScript {
};
}  // namespace

TEST_P(MessageChannelTestWithJavaScript, MessageChannelIsConstructible) {
  std::string result;
  EXPECT_TRUE(EvaluateScript("var channel = new MessageChannel();", &result))
      << "Failed to evaluate script.";
  EXPECT_EQ("undefined", result) << result;
}

TEST_P(MessageChannelTestWithJavaScript, MessageChannelPort1IsMessagePort) {
  std::string result;
  EXPECT_TRUE(EvaluateScript(
      "var channel = new MessageChannel(); channel.port1", &result))
      << "Failed to evaluate script.";
  EXPECT_NE("null", result) << result;
  EXPECT_EQ("[object MessagePort]", result) << result;
}

TEST_P(MessageChannelTestWithJavaScript, MessageChannelPort2IsMessagePort) {
  std::string result;
  EXPECT_TRUE(EvaluateScript(
      "var channel = new MessageChannel(); channel.port2", &result))
      << "Failed to evaluate script.";
  EXPECT_NE("null", result) << result;
  EXPECT_EQ("[object MessagePort]", result) << result;
}

INSTANTIATE_TEST_CASE_P(
    MessageChannelTestsWithJavaScript, MessageChannelTestWithJavaScript,
    ::testing::ValuesIn(testing::TestWebWithJavaScript::GetWebTypes()),
    testing::TestWebWithJavaScript::GetTypeName);

}  // namespace web
}  // namespace cobalt
