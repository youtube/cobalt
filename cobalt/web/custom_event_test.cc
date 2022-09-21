// Copyright 2017 The Cobalt Authors. All Rights Reserved.
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

#include "cobalt/web/custom_event.h"

#include <memory>
#include <string>

#include "base/bind.h"
#include "base/logging.h"
#include "cobalt/web/custom_event_init.h"
#include "cobalt/web/testing/gtest_workarounds.h"
#include "cobalt/web/testing/test_with_javascript.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;

namespace cobalt {
namespace web {

namespace {
class CustomEventTestWithJavaScript : public testing::TestWebWithJavaScript {};
}  // namespace

TEST(CustomEventTest, ConstructorWithEventTypeString) {
  scoped_refptr<CustomEvent> event = new CustomEvent("mytestevent");

  EXPECT_EQ("mytestevent", event->type());
  EXPECT_EQ(NULL, event->target().get());
  EXPECT_EQ(NULL, event->current_target().get());
  EXPECT_EQ(web::Event::kNone, event->event_phase());
  EXPECT_FALSE(event->bubbles());
  EXPECT_FALSE(event->cancelable());
  EXPECT_FALSE(event->default_prevented());
  EXPECT_FALSE(event->IsBeingDispatched());
  EXPECT_FALSE(event->propagation_stopped());
  EXPECT_FALSE(event->immediate_propagation_stopped());
  EXPECT_EQ(NULL, event->detail());
}

TEST(CustomEventTest, ConstructorWithEventTypeAndDefaultInitDict) {
  CustomEventInit init;
  scoped_refptr<CustomEvent> event = new CustomEvent("mytestevent", init);

  EXPECT_EQ("mytestevent", event->type());
  EXPECT_EQ(NULL, event->target().get());
  EXPECT_EQ(NULL, event->current_target().get());
  EXPECT_EQ(web::Event::kNone, event->event_phase());
  EXPECT_FALSE(event->bubbles());
  EXPECT_FALSE(event->cancelable());
  EXPECT_FALSE(event->default_prevented());
  EXPECT_FALSE(event->IsBeingDispatched());
  EXPECT_FALSE(event->propagation_stopped());
  EXPECT_FALSE(event->immediate_propagation_stopped());
  EXPECT_EQ(NULL, event->detail());
}

TEST_P(CustomEventTestWithJavaScript,
       ConstructorWithEventTypeAndCustomInitDict) {
  std::string result;
  bool success = EvaluateScript(
      "var event = new CustomEvent('dog', "
      "    {'bubbles':true, "
      "     'cancelable':true, "
      "     'detail':{'cobalt':'rulez'}});"
      "if (event.type == 'dog' &&"
      "    event.bubbles == true &&"
      "    event.cancelable == true) "
      "    event.detail.cobalt;",
      &result);
  EXPECT_EQ("rulez", result);

  if (!success) {
    DLOG(ERROR) << "Failed to evaluate test: "
                << "\"" << result << "\"";
  }
}

TEST_P(CustomEventTestWithJavaScript, InitCustomEvent) {
  std::string result;
  bool success = EvaluateScript(
      "var event = new CustomEvent('cat');\n"
      "event.initCustomEvent('dog', true, true, {cobalt:'rulez'});"
      "if (event.type == 'dog' &&"
      "    event.detail &&"
      "    event.bubbles == true &&"
      "    event.cancelable == true) "
      "    event.detail.cobalt;",
      &result);
  EXPECT_EQ("rulez", result);

  if (!success) {
    DLOG(ERROR) << "Failed to evaluate test: "
                << "\"" << result << "\"";
  }
}

INSTANTIATE_TEST_CASE_P(
    CustomEventTestsWithJavaScript, CustomEventTestWithJavaScript,
    ::testing::ValuesIn(testing::TestWebWithJavaScript::GetWebTypes()),
    testing::TestWebWithJavaScript::GetTypeName);

}  // namespace web
}  // namespace cobalt
