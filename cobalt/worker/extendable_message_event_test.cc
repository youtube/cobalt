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

#include "cobalt/worker/extendable_message_event.h"

#include <memory>
#include <string>

#include "base/bind.h"
#include "base/logging.h"
#include "base/memory/scoped_refptr.h"
#include "cobalt/script/testing/fake_script_value.h"
#include "cobalt/script/v8c/entry_scope.h"
#include "cobalt/script/value_handle.h"
#include "cobalt/web/testing/gtest_workarounds.h"
#include "cobalt/web/testing/mock_event_listener.h"
#include "cobalt/worker/extendable_message_event_init.h"
#include "cobalt/worker/testing/test_with_javascript.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cobalt {
namespace worker {

using script::testing::FakeScriptValue;
using ::testing::IsSubstring;
using web::testing::MockEventListener;

namespace {
class ExtendableMessageEventTestWithJavaScript
    : public testing::TestServiceWorkerWithJavaScript {
 protected:
  ExtendableMessageEventTestWithJavaScript() {
    fake_event_listener_ = MockEventListener::Create();
  }

  ~ExtendableMessageEventTestWithJavaScript() override {}

  std::unique_ptr<MockEventListener> fake_event_listener_;
};
}  // namespace

TEST_F(ExtendableMessageEventTestWithJavaScript,
       ConstructorWithEventTypeString) {
  scoped_refptr<ExtendableMessageEvent> event =
      new ExtendableMessageEvent("mytestevent");

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
  script::Handle<script::ValueHandle> event_data =
      event->data(web_context()->environment_settings());
  EXPECT_TRUE(event_data.IsEmpty());
}

TEST_F(ExtendableMessageEventTestWithJavaScript, ConstructorWithAny) {
  base::Optional<script::ValueHandleHolder::Reference> reference;
  EvaluateScript("'ConstructorWithAnyMessageData'", &reference);
  std::unique_ptr<script::DataBuffer> data(
      script::SerializeScriptValue(reference->referenced_value()));
  EXPECT_NE(nullptr, data.get());
  EXPECT_NE(nullptr, data->ptr);
  EXPECT_GT(data->size, 0U);
  const ExtendableMessageEventInit init;
  scoped_refptr<ExtendableMessageEvent> event = new ExtendableMessageEvent(
      base::Tokens::message(), init, std::move(data));

  EXPECT_EQ("message", event->type());
  EXPECT_EQ(NULL, event->target().get());
  EXPECT_EQ(NULL, event->current_target().get());
  EXPECT_EQ(web::Event::kNone, event->event_phase());
  EXPECT_FALSE(event->bubbles());
  EXPECT_FALSE(event->cancelable());
  EXPECT_FALSE(event->default_prevented());
  EXPECT_FALSE(event->IsBeingDispatched());
  EXPECT_FALSE(event->propagation_stopped());
  EXPECT_FALSE(event->immediate_propagation_stopped());
  script::Handle<script::ValueHandle> event_data =
      event->data(web_context()->environment_settings());
  EXPECT_FALSE(event_data.IsEmpty());
  auto script_value = event_data.GetScriptValue();
  auto* isolate = script::GetIsolate(*script_value);
  script::v8c::EntryScope entry_scope(isolate);
  v8::Local<v8::Value> v8_value = script::GetV8Value(*script_value);
  std::string actual =
      *(v8::String::Utf8Value(isolate, v8_value.As<v8::String>()));
  EXPECT_EQ("ConstructorWithAnyMessageData", actual);
}

TEST_F(ExtendableMessageEventTestWithJavaScript,
       ConstructorWithEventTypeAndDefaultInitDict) {
  ExtendableMessageEventInit init;
  scoped_refptr<ExtendableMessageEvent> event =
      new ExtendableMessageEvent("mytestevent", init);

  EXPECT_EQ("mytestevent", event->type());
  EXPECT_EQ(nullptr, event->target().get());
  EXPECT_EQ(nullptr, event->current_target().get());
  EXPECT_EQ(web::Event::kNone, event->event_phase());
  EXPECT_FALSE(event->bubbles());
  EXPECT_FALSE(event->cancelable());
  EXPECT_FALSE(event->default_prevented());
  EXPECT_FALSE(event->IsBeingDispatched());
  EXPECT_FALSE(event->propagation_stopped());
  EXPECT_FALSE(event->immediate_propagation_stopped());

  script::Handle<script::ValueHandle> event_data =
      event->data(web_context()->environment_settings());
  EXPECT_TRUE(event_data.IsEmpty());
  EXPECT_TRUE(event->origin().empty());
  EXPECT_TRUE(event->last_event_id().empty());
  EXPECT_FALSE(event->source().IsType<scoped_refptr<Client>>());
  EXPECT_FALSE(event->source().IsType<scoped_refptr<ServiceWorker>>());
  EXPECT_FALSE(event->source().IsType<scoped_refptr<web::MessagePort>>());
  EXPECT_TRUE(event->ports().empty());
}

TEST_F(ExtendableMessageEventTestWithJavaScript,
       ConstructorWithEventTypeAndInitDict) {
  ExtendableMessageEventInit init;
  base::Optional<script::ValueHandleHolder::Reference> reference;
  EvaluateScript("'data_value'", &reference);
  init.set_data(&(reference->referenced_value()));
  init.set_origin("OriginString");
  init.set_last_event_id("lastEventIdString");
  base::Optional<ExtendableMessageEvent::SourceType> client(
      Client::Create(web_context()->environment_settings()));
  init.set_source(client);
  scoped_refptr<ExtendableMessageEvent> event =
      new ExtendableMessageEvent("mytestevent", init);

  EXPECT_EQ("mytestevent", event->type());
  EXPECT_EQ(nullptr, event->target().get());
  EXPECT_EQ(nullptr, event->current_target().get());
  EXPECT_EQ(web::Event::kNone, event->event_phase());
  EXPECT_FALSE(event->bubbles());
  EXPECT_FALSE(event->cancelable());
  EXPECT_FALSE(event->default_prevented());
  EXPECT_FALSE(event->IsBeingDispatched());
  EXPECT_FALSE(event->propagation_stopped());
  EXPECT_FALSE(event->immediate_propagation_stopped());

  script::Handle<script::ValueHandle> event_data =
      event->data(web_context()->environment_settings());
  auto script_value = event_data.GetScriptValue();
  auto* isolate = script::GetIsolate(*script_value);
  script::v8c::EntryScope entry_scope(isolate);
  v8::Local<v8::Value> v8_value = script::GetV8Value(*script_value);
  std::string actual =
      *(v8::String::Utf8Value(isolate, v8_value.As<v8::String>()));
  EXPECT_EQ("data_value", actual);

  EXPECT_EQ("OriginString", event->origin());
  EXPECT_EQ("lastEventIdString", event->last_event_id());
  EXPECT_TRUE(event->source().IsType<scoped_refptr<Client>>());
  EXPECT_EQ(client.value().AsType<scoped_refptr<Client>>().get(),
            event->source().AsType<scoped_refptr<Client>>().get());
  EXPECT_TRUE(event->ports().empty());
}

TEST_F(ExtendableMessageEventTestWithJavaScript,
       ConstructorWithEventTypeAndInvalidSource) {
  std::string result;
  EXPECT_FALSE(
      EvaluateScript("var event = new ExtendableMessageEvent('dog', "
                     "    {source: this});"
                     "if (event.type == 'dog') event.source;",
                     &result))
      << "Failed to evaluate script.";
  EXPECT_PRED_FORMAT2(IsSubstring,
                      "TypeError: Value is not a member of the union type.",
                      result);
  // EXPECT_FALSE(IsSubstring(
  //    "", "", "TypeError: Value is not a member of the union type.", result));
}


TEST_F(ExtendableMessageEventTestWithJavaScript,
       ConstructorWithEventTypeAndExtendableMessageEventInitDict) {
  std::string result;
  // Note: None of the types for 'source' are constructible, so that can
  // not easily be tested here.
  EXPECT_TRUE(
      EvaluateScript("var event = new ExtendableMessageEvent('dog', "
                     "    {cancelable: true, "
                     "     origin: 'OriginValue',"
                     "     lastEventId: 'LastEventIdValue',"
                     "     data: {value: 'data_value'},"
                     "    }"
                     ");"
                     "if (event.type == 'dog' &&"
                     "    event.bubbles == false &&"
                     "    event.cancelable == true &&"
                     "    event.origin == 'OriginValue' &&"
                     "    event.lastEventId == 'LastEventIdValue' &&"
                     "    event.ports.length == 0 &&"
                     "    event.data.value == 'data_value') "
                     "    event.data.value;",
                     &result))
      << "Failed to evaluate script.";
  EXPECT_EQ("data_value", result);
}

}  // namespace worker
}  // namespace cobalt
