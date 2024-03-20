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

#include "cobalt/web/error_event.h"

#include <memory>
#include <string>

#include "base/bind.h"
#include "base/logging.h"
#include "base/test/task_environment.h"
#include "cobalt/script/testing/fake_script_value.h"
#include "cobalt/web/error_event_init.h"
#include "cobalt/web/testing/gtest_workarounds.h"
#include "cobalt/web/testing/mock_event_listener.h"
#include "cobalt/web/testing/test_with_javascript.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cobalt {
namespace web {

using ::testing::ContainsRegex;

namespace {
class ErrorEventTestWithJavaScript : public testing::TestWebWithJavaScript {
 protected:
  ErrorEventTestWithJavaScript() {
    fake_event_listener_ = testing::MockEventListener::Create();
  }

  std::unique_ptr<testing::MockEventListener> fake_event_listener_;
};
}  // namespace

TEST(ErrorEventTest, ConstructorWithEventTypeString) {
  scoped_refptr<ErrorEvent> event =
      new ErrorEvent(/*environment_settings=*/nullptr, "mytestevent");

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
  EXPECT_EQ("", event->message());
  EXPECT_EQ("", event->filename());
  EXPECT_EQ(0, event->lineno());
  EXPECT_EQ(0, event->colno());
  EXPECT_EQ(NULL, event->error());
}

TEST(ErrorEventTest, ConstructorWithEventTypeAndDefaultInitDict) {
  ErrorEventInit init;
  scoped_refptr<ErrorEvent> event =
      new ErrorEvent(/*environment_settings=*/nullptr, "mytestevent", init);

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
  EXPECT_EQ("", event->message());
  EXPECT_EQ("", event->filename());
  EXPECT_EQ(0, event->lineno());
  EXPECT_EQ(0, event->colno());
  EXPECT_EQ(NULL, event->error());
}

TEST_P(ErrorEventTestWithJavaScript, ConstructorWithEventTypeAndErrorInitDict) {
  std::string result;
  bool success = EvaluateScript(
      "var event = new ErrorEvent('dog', "
      "    {'cancelable':true, "
      "     'message':'error_message', "
      "     'filename':'error_filename', "
      "     'lineno':100, "
      "     'colno':50, "
      "     'error':{'cobalt':'rulez'}});"
      "if (event.type == 'dog' &&"
      "    event.bubbles == false &&"
      "    event.cancelable == true &&"
      "    event.message == 'error_message' &&"
      "    event.filename == 'error_filename' &&"
      "    event.lineno == 100 &&"
      "    event.colno == 50) "
      "    event.error.cobalt;",
      &result);
  EXPECT_EQ("rulez", result);

  if (!success) {
    DLOG(ERROR) << "Failed to evaluate test: "
                << "\"" << result << "\"";
  } else {
    LOG(INFO) << "Test result : "
              << "\"" << result << "\"";
  }
}

TEST_P(ErrorEventTestWithJavaScript, ErrorEventIsDispatched) {
  GetWindowOrWorkerGlobalScope()->AddEventListener(
      "error",
      script::testing::FakeScriptValue<web::EventListener>(
          fake_event_listener_.get()),
      true);
  fake_event_listener_->ExpectHandleEventCall("error",
                                              GetWindowOrWorkerGlobalScope());
  GetWindowOrWorkerGlobalScope()->DispatchEvent(new web::Event("error"));
}

TEST_P(ErrorEventTestWithJavaScript, ErrorEventIsFiredFromSyntaxError) {
  GetWindowOrWorkerGlobalScope()->AddEventListener(
      "error",
      script::testing::FakeScriptValue<web::EventListener>(
          fake_event_listener_.get()),
      true);
  fake_event_listener_->ExpectHandleEventCall("error",
                                              GetWindowOrWorkerGlobalScope());
  std::string result;
  bool success = EvaluateScript("my_syntax_error_here", &result);
  EXPECT_THAT(result, ContainsRegex("my_syntax_error_here"));
  EXPECT_FALSE(success);
}

TEST_P(ErrorEventTestWithJavaScript, ErrorEventIsFiredFromThrow) {
  GetWindowOrWorkerGlobalScope()->AddEventListener(
      "error",
      script::testing::FakeScriptValue<web::EventListener>(
          fake_event_listener_.get()),
      true);
  fake_event_listener_->ExpectHandleEventCall("error",
                                              GetWindowOrWorkerGlobalScope());
  std::string result;
  bool success =
      EvaluateScript("throw (new Error('my_thrown_error'))", &result);
  EXPECT_THAT(result, ContainsRegex("my_thrown_error"));
  EXPECT_FALSE(success);
}

INSTANTIATE_TEST_CASE_P(
    ErrorEventTestsWithJavaScript, ErrorEventTestWithJavaScript,
    ::testing::ValuesIn(testing::TestWebWithJavaScript::GetWebTypes()),
    testing::TestWebWithJavaScript::GetTypeName);

}  // namespace web
}  // namespace cobalt
