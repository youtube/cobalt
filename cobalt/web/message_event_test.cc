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

#include "cobalt/web/message_event.h"

#include <memory>
#include <string>

#include "base/bind.h"
#include "base/logging.h"
#include "cobalt/dom/testing/test_with_javascript.h"
#include "cobalt/web/message_event_init.h"
#include "cobalt/web/testing/gtest_workarounds.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cobalt {
namespace web {

namespace {
class MessageEventTestWithJavaScript : public dom::testing::TestWithJavaScript {
};
}  // namespace

TEST(MessageEventTest, ConstructorWithEventTypeString) {
  scoped_refptr<MessageEvent> event = new MessageEvent("mytestevent");

  EXPECT_EQ("mytestevent", event->type());
  EXPECT_EQ(NULL, event->target().get());
  EXPECT_EQ(NULL, event->current_target().get());
  EXPECT_EQ(Event::kNone, event->event_phase());
  EXPECT_FALSE(event->bubbles());
  EXPECT_FALSE(event->cancelable());
  EXPECT_FALSE(event->default_prevented());
  EXPECT_FALSE(event->IsBeingDispatched());
  EXPECT_FALSE(event->propagation_stopped());
  EXPECT_FALSE(event->immediate_propagation_stopped());
  MessageEvent::ResponseType event_data = event->data();
  EXPECT_TRUE(event_data.IsType<std::string>());
  EXPECT_EQ("", event_data.AsType<std::string>());
}

TEST(MessageEventTest, ConstructorWithEventTypeAndDefaultInitDict) {
  MessageEventInit init;
  init.set_data("data_value");
  scoped_refptr<MessageEvent> event = new MessageEvent("mytestevent", init);

  EXPECT_EQ("mytestevent", event->type());
  EXPECT_EQ(NULL, event->target().get());
  EXPECT_EQ(NULL, event->current_target().get());
  EXPECT_EQ(Event::kNone, event->event_phase());
  EXPECT_FALSE(event->bubbles());
  EXPECT_FALSE(event->cancelable());
  EXPECT_FALSE(event->default_prevented());
  EXPECT_FALSE(event->IsBeingDispatched());
  EXPECT_FALSE(event->propagation_stopped());
  EXPECT_FALSE(event->immediate_propagation_stopped());
  MessageEvent::ResponseType event_data = event->data();
  EXPECT_TRUE(event_data.IsType<std::string>());
  EXPECT_EQ("data_value", event_data.AsType<std::string>());
}

TEST_F(MessageEventTestWithJavaScript,
       ConstructorWithEventTypeAndErrorInitDict) {
  std::string result;
  bool success = EvaluateScript(
      "var event = new MessageEvent('dog', "
      "    {'cancelable':true, "
      "     'data':'data_value'});"
      "if (event.type == 'dog' &&"
      "    event.bubbles == false &&"
      "    event.cancelable == true &&"
      "    event.data == 'data_value') "
      "    event.data;",
      &result);
  EXPECT_EQ("data_value", result);

  if (!success) {
    DLOG(ERROR) << "Failed to evaluate test: "
                << "\"" << result << "\"";
  } else {
    LOG(INFO) << "Test result : "
              << "\"" << result << "\"";
  }
}

}  // namespace web
}  // namespace cobalt
