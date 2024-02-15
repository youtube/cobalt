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

#include "cobalt/web/message_port.h"

#include "base/strings/string_util.h"
#include "cobalt/web/testing/test_with_javascript.h"


#define EXPECT_SUBSTRING(needle, haystack) \
  EXPECT_PRED_FORMAT2(::testing::IsSubstring, (needle), (haystack))

namespace cobalt {
namespace web {

namespace {
class MessagePortTestWithJavaScript : public testing::TestWebWithJavaScript {};
}  // namespace

TEST_P(MessagePortTestWithJavaScript, MessagePortIsNotConstructible) {
  std::string result;
  EXPECT_FALSE(EvaluateScript("var event = new MessagePort();", &result))
      << "Failed to evaluate script.";
  EXPECT_SUBSTRING("TypeError: MessagePort is not constructible", result)
      << result;
}

INSTANTIATE_TEST_CASE_P(
    MessagePortTestsWithJavaScript, MessagePortTestWithJavaScript,
    ::testing::ValuesIn(testing::TestWebWithJavaScript::GetWebTypes()),
    testing::TestWebWithJavaScript::GetTypeName);

}  // namespace web
}  // namespace cobalt
