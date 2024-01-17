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

#include "base/logging.h"
#include "base/memory/scoped_refptr.h"
#include "base/strings/string_util.h"
#include "cobalt/script/testing/fake_script_value.h"
#include "cobalt/script/testing/mock_exception_state.h"
#include "cobalt/web/event_init.h"
#include "cobalt/web/event_target.h"
#include "cobalt/web/message_event.h"
#include "cobalt/web/testing/mock_event_listener.h"
#include "cobalt/web/testing/stub_environment_settings.h"
#include "cobalt/web/testing/test_with_javascript.h"
#include "testing/gtest/include/gtest/gtest.h"

#define EXPECT_SUBSTRING(needle, haystack) \
  EXPECT_PRED_FORMAT2(::testing::IsSubstring, (needle), (haystack))


using ::testing::_;
using ::testing::AllOf;
using ::testing::Eq;
using ::testing::Property;
using ::testing::Return;

namespace cobalt {
namespace web {

using script::testing::FakeScriptValue;
using script::testing::MockExceptionState;
using ::testing::StrictMock;

namespace {
class MessagePortTest : public ::testing::Test {
 public:
  MessagePortTest() {
    port_ = new MessagePort;
    fake_event_listener_ = web::testing::MockEventListener::Create();
    web_context_.reset(new web::testing::StubWebContext());
    web_context_->SetupEnvironmentSettings(
        new dom::testing::StubEnvironmentSettings());
    event_target_ = new EventTarget(web_context_->environment_settings());
  }

 protected:
  scoped_refptr<MessagePort> port_;
  std::unique_ptr<web::testing::MockEventListener> fake_event_listener_;
  std::unique_ptr<web::Context> web_context_;
  scoped_refptr<EventTarget> event_target_;
  StrictMock<MockExceptionState> exception_state_;
};

class WithJavaScript : public testing::TestWebWithJavaScript {
 public:
  WithJavaScript() {
    port_ = new MessagePort;
    fake_event_listener_ = web::testing::MockEventListener::Create();
    event_target_ = new EventTarget(environment_settings());
  }

 protected:
  std::unique_ptr<web::testing::MockEventListener> fake_event_listener_;
  scoped_refptr<MessagePort> port_;
  scoped_refptr<EventTarget> event_target_;
};
}  // namespace

TEST_F(MessagePortTest, StartingDoesNotEnableUnentangledPort) {
  EXPECT_FALSE(port_->enabled());
  port_->Start();
  EXPECT_FALSE(port_->enabled());
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(port_->enabled());
  base::RunLoop().RunUntilIdle();
}

TEST_F(MessagePortTest, EntangleWithEventTarget) {
  EXPECT_NE(port_->event_target(), event_target_.get());
  port_->EntangleWithEventTarget(event_target_.get());

  EXPECT_FALSE(port_->enabled());
  EXPECT_EQ(port_->event_target(), event_target_.get());
  port_.reset();
  base::RunLoop().RunUntilIdle();
}

TEST_F(MessagePortTest, StartingEnablesEntangledPortImmediately) {
  port_->EntangleWithEventTarget(event_target_.get());

  EXPECT_FALSE(port_->enabled());
  port_->Start();
  EXPECT_TRUE(port_->enabled());
  port_.reset();
  base::RunLoop().RunUntilIdle();
}

TEST_F(MessagePortTest, EntanglingWithNull) {
  port_->EntangleWithEventTarget(event_target_.get());

  EXPECT_FALSE(port_->enabled());
  port_->Start();
  EXPECT_TRUE(port_->enabled());
  port_->EntangleWithEventTarget(nullptr);
  port_.reset();
  base::RunLoop().RunUntilIdle();
}

TEST_F(MessagePortTest,
       AddingMessageListenerToEventHandlerEnablesEntangledPort) {
  fake_event_listener_->ExpectNoHandleEventCall();

  port_->EntangleWithEventTarget(event_target_.get());

  EXPECT_FALSE(port_->enabled());
  event_target_->AddEventListener(
      "message", FakeScriptValue<EventListener>(fake_event_listener_.get()),
      false);
  EXPECT_FALSE(port_->enabled());
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(port_->enabled());
  base::RunLoop().RunUntilIdle();
  port_.reset();
}

TEST_F(MessagePortTest, AddingMessageListenerEnablesEntangledPort) {
  fake_event_listener_->ExpectNoHandleEventCall();

  port_->EntangleWithEventTarget(event_target_.get());

  EXPECT_FALSE(port_->enabled());
  port_->AddEventListener(
      "message", FakeScriptValue<EventListener>(fake_event_listener_.get()),
      false);
  EXPECT_TRUE(event_target_->HasEventListener("message"));
  EXPECT_FALSE(port_->enabled());
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(port_->enabled());
  base::RunLoop().RunUntilIdle();
  port_.reset();
}

TEST_F(MessagePortTest, RemoveMessageListener) {
  fake_event_listener_->ExpectNoHandleEventCall();

  port_->EntangleWithEventTarget(event_target_.get());

  EXPECT_FALSE(port_->enabled());
  port_->AddEventListener(
      "message", FakeScriptValue<EventListener>(fake_event_listener_.get()),
      false);
  EXPECT_TRUE(event_target_->HasEventListener("message"));
  EXPECT_FALSE(port_->enabled());
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(port_->enabled());
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(event_target_->HasEventListener("message"));
  port_->RemoveEventListener(
      "message", FakeScriptValue<EventListener>(fake_event_listener_.get()),
      false);
  EXPECT_FALSE(event_target_->HasEventListener("message"));
  port_->DispatchEvent(new web::MessageEvent("message"), &exception_state_);

  EXPECT_TRUE(port_->enabled());
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(port_->enabled());
  base::RunLoop().RunUntilIdle();

  port_.reset();
}

TEST_F(MessagePortTest, AddingMessageErrorListenerDoesNotEnableEntangledPort) {
  fake_event_listener_->ExpectNoHandleEventCall();

  port_->EntangleWithEventTarget(event_target_.get());

  EXPECT_FALSE(port_->enabled());
  port_->AddEventListener(
      "messageerror",
      FakeScriptValue<EventListener>(fake_event_listener_.get()), false);
  EXPECT_TRUE(event_target_->HasEventListener("messageerror"));
  EXPECT_FALSE(port_->enabled());
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(port_->enabled());
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(port_->enabled());
  port_.reset();
}

TEST_F(MessagePortTest, MessageEventListenBeforeEntangling) {
  EXPECT_CALL(exception_state_, SetException(_)).Times(0);
  fake_event_listener_->ExpectNoHandleEventCall();

  EXPECT_FALSE(port_->enabled());
  port_->AddEventListener(
      "message", FakeScriptValue<EventListener>(fake_event_listener_.get()),
      false);

  port_->EntangleWithEventTarget(event_target_.get());

  EXPECT_FALSE(port_->enabled());
  port_->DispatchEvent(new web::MessageEvent("message"), &exception_state_);
  EXPECT_FALSE(port_->enabled());
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(port_->enabled());
  base::RunLoop().RunUntilIdle();

  port_.reset();
}

TEST_F(MessagePortTest, MessageEventRemoveWithoutEntangling) {
  EXPECT_CALL(exception_state_, SetException(_)).Times(0);
  fake_event_listener_->ExpectNoHandleEventCall();

  EXPECT_FALSE(port_->enabled());
  port_->RemoveEventListener(
      "message", FakeScriptValue<EventListener>(fake_event_listener_.get()),
      false);
}

TEST_F(MessagePortTest, MessageEvent) {
  EXPECT_CALL(exception_state_, SetException(_)).Times(0);
  fake_event_listener_->ExpectHandleEventCall("message", event_target_.get());

  port_->EntangleWithEventTarget(event_target_.get());

  EXPECT_FALSE(port_->enabled());
  port_->AddEventListener(
      "message", FakeScriptValue<EventListener>(fake_event_listener_.get()),
      false);
  port_->DispatchEvent(new web::MessageEvent("message"), &exception_state_);
  EXPECT_FALSE(port_->enabled());
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(port_->enabled());
  base::RunLoop().RunUntilIdle();

  port_.reset();
}

TEST_F(MessagePortTest, OnMessageEvent) {
  EXPECT_CALL(exception_state_, SetException(_)).Times(0);
  fake_event_listener_->ExpectHandleEventCall("message", event_target_.get());

  EXPECT_FALSE(port_->onmessage());
  port_->set_onmessage(
      FakeScriptValue<EventListener>(fake_event_listener_.get()));
  EXPECT_FALSE(port_->onmessage());
  EXPECT_FALSE(event_target_->HasOneOrMoreAttributeEventListener());

  port_->EntangleWithEventTarget(event_target_.get());

  EXPECT_FALSE(port_->enabled());
  EXPECT_FALSE(port_->onmessage());
  port_->set_onmessage(
      FakeScriptValue<EventListener>(fake_event_listener_.get()));
  EXPECT_TRUE(port_->onmessage());
  EXPECT_TRUE(event_target_->HasOneOrMoreAttributeEventListener());
  EXPECT_TRUE(
      event_target_->GetAttributeEventListener(base::Tokens::message()));
  port_->DispatchEvent(new web::MessageEvent("message"), &exception_state_);
  EXPECT_FALSE(port_->enabled());
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(port_->enabled());
  base::RunLoop().RunUntilIdle();
  port_.reset();
}

TEST_F(MessagePortTest, MessageErrorEvent) {
  EXPECT_CALL(exception_state_, SetException(_)).Times(0);
  fake_event_listener_->ExpectHandleEventCall("messageerror",
                                              event_target_.get());

  port_->EntangleWithEventTarget(event_target_.get());
  EXPECT_FALSE(port_->enabled());
  port_->AddEventListener(
      "messageerror",
      FakeScriptValue<EventListener>(fake_event_listener_.get()), false);
  port_->DispatchEvent(new web::MessageEvent("messageerror"),
                       &exception_state_);
  EXPECT_FALSE(port_->enabled());
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(port_->enabled());
  port_->Start();  // Start explicitly because there isn't a message listener.
  EXPECT_TRUE(port_->enabled());
  base::RunLoop().RunUntilIdle();
  port_.reset();
}

TEST_F(MessagePortTest, OnMessageErrorEvent) {
  EXPECT_CALL(exception_state_, SetException(_)).Times(0);
  fake_event_listener_->ExpectHandleEventCall("messageerror",
                                              event_target_.get());

  EXPECT_FALSE(port_->onmessageerror());
  port_->set_onmessageerror(
      FakeScriptValue<EventListener>(fake_event_listener_.get()));
  EXPECT_FALSE(port_->onmessageerror());
  EXPECT_FALSE(event_target_->HasOneOrMoreAttributeEventListener());

  port_->EntangleWithEventTarget(event_target_.get());

  EXPECT_FALSE(port_->enabled());
  EXPECT_FALSE(port_->onmessageerror());
  port_->set_onmessageerror(
      FakeScriptValue<EventListener>(fake_event_listener_.get()));
  EXPECT_TRUE(port_->onmessageerror());
  EXPECT_TRUE(event_target_->HasOneOrMoreAttributeEventListener());
  EXPECT_TRUE(
      event_target_->GetAttributeEventListener(base::Tokens::messageerror()));
  port_->DispatchEvent(new web::MessageEvent("messageerror"),
                       &exception_state_);
  EXPECT_FALSE(port_->enabled());
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(port_->enabled());
  port_->Start();  // Start explicitly because there isn't a message listener.
  EXPECT_TRUE(port_->enabled());
  base::RunLoop().RunUntilIdle();
  port_.reset();
}


TEST_P(WithJavaScript, PostMessage) {
  fake_event_listener_->ExpectHandleEventCall("message", event_target_);

  port_->EntangleWithEventTarget(event_target_.get());

  event_target_->AddEventListener(
      "message", FakeScriptValue<EventListener>(fake_event_listener_.get()),
      false);
  EXPECT_FALSE(port_->enabled());
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(port_->enabled());

  absl::optional<script::ValueHandleHolder::Reference> reference;
  EvaluateScript("'PostMessageTestMessageString'", &reference);
  port_->PostMessage(reference->referenced_value());

  base::RunLoop().RunUntilIdle();
  port_.reset();
}

TEST_P(WithJavaScript, PostFailedMessage) {
  fake_event_listener_->ExpectNoHandleEventCall();

  port_->EntangleWithEventTarget(event_target_.get());

  event_target_->AddEventListener(
      "message", FakeScriptValue<EventListener>(fake_event_listener_.get()),
      false);
  EXPECT_FALSE(port_->enabled());
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(port_->enabled());

  absl::optional<script::ValueHandleHolder::Reference> reference;
  EvaluateScript("new Function", &reference);
  port_->PostMessage(reference->referenced_value());

  EvaluateScript("'PostFailedMessage'", &reference);

  base::RunLoop().RunUntilIdle();
  port_.reset();
}

TEST_P(WithJavaScript, PostMessageBeforeListening) {
  fake_event_listener_->ExpectHandleEventCall("message", event_target_);

  port_->EntangleWithEventTarget(event_target_.get());

  absl::optional<script::ValueHandleHolder::Reference> reference;
  EvaluateScript("'PostMessageTestMessageString'", &reference);
  port_->PostMessage(reference->referenced_value());

  event_target_->AddEventListener(
      "message", FakeScriptValue<EventListener>(fake_event_listener_.get()),
      false);

  EXPECT_FALSE(port_->enabled());
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(port_->enabled());

  base::RunLoop().RunUntilIdle();
  port_.reset();
}

TEST_P(WithJavaScript, MessagePortIsNotConstructible) {
  std::string result;
  EXPECT_FALSE(EvaluateScript("var event = new MessagePort();", &result))
      << "Failed to evaluate script.";
  EXPECT_SUBSTRING("TypeError: MessagePort is not constructible", result)
      << result;
}

INSTANTIATE_TEST_CASE_P(
    MessagePortTest, WithJavaScript,
    ::testing::ValuesIn(testing::TestWebWithJavaScript::GetWebTypes()),
    testing::TestWebWithJavaScript::GetTypeName);

}  // namespace web
}  // namespace cobalt
