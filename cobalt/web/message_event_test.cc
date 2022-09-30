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
#include "base/memory/scoped_refptr.h"
#include "cobalt/script/v8c/entry_scope.h"
#include "cobalt/script/value_handle.h"
#include "cobalt/web/blob.h"
#include "cobalt/web/message_event_init.h"
#include "cobalt/web/testing/gtest_workarounds.h"
#include "cobalt/web/testing/test_with_javascript.h"
#include "net/base/io_buffer.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cobalt {
namespace web {

namespace {
class MessageEventTestWithJavaScript : public testing::TestWebWithJavaScript {};
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
  MessageEvent::Response event_data = event->data();
  EXPECT_TRUE(event_data.IsType<script::Handle<script::ValueHandle>>());
  EXPECT_TRUE(
      event_data.AsType<script::Handle<script::ValueHandle>>().IsEmpty());
}

TEST(MessageEventTest, ConstructorWithText) {
  std::string message_string("ConstructorWithTextMessageData");
  scoped_refptr<net::IOBufferWithSize> data =
      base::MakeRefCounted<net::IOBufferWithSize>(message_string.size());
  memcpy(data->data(), message_string.c_str(), message_string.size());
  scoped_refptr<MessageEvent> event =
      new MessageEvent(base::Tokens::message(), MessageEvent::kText, data);

  EXPECT_EQ("message", event->type());
  EXPECT_EQ(NULL, event->target().get());
  EXPECT_EQ(NULL, event->current_target().get());
  EXPECT_EQ(Event::kNone, event->event_phase());
  EXPECT_FALSE(event->bubbles());
  EXPECT_FALSE(event->cancelable());
  EXPECT_FALSE(event->default_prevented());
  EXPECT_FALSE(event->IsBeingDispatched());
  EXPECT_FALSE(event->propagation_stopped());
  EXPECT_FALSE(event->immediate_propagation_stopped());
  MessageEvent::Response event_data = event->data();
  EXPECT_TRUE(event_data.IsType<std::string>());
  EXPECT_EQ(message_string.size(), event_data.AsType<std::string>().size());
  EXPECT_EQ("ConstructorWithTextMessageData", event_data.AsType<std::string>());
}

TEST_P(MessageEventTestWithJavaScript, ConstructorWithBlob) {
  std::string message_string("ConstructorWithBlobMessageData");
  scoped_refptr<net::IOBufferWithSize> data =
      base::MakeRefCounted<net::IOBufferWithSize>(message_string.size());
  memcpy(data->data(), message_string.c_str(), message_string.size());
  scoped_refptr<MessageEvent> event =
      new MessageEvent(base::Tokens::message(), MessageEvent::kBlob, data);

  EXPECT_EQ("message", event->type());
  EXPECT_EQ(NULL, event->target().get());
  EXPECT_EQ(NULL, event->current_target().get());
  EXPECT_EQ(Event::kNone, event->event_phase());
  EXPECT_FALSE(event->bubbles());
  EXPECT_FALSE(event->cancelable());
  EXPECT_FALSE(event->default_prevented());
  EXPECT_FALSE(event->IsBeingDispatched());
  EXPECT_FALSE(event->propagation_stopped());
  EXPECT_FALSE(event->immediate_propagation_stopped());
  MessageEvent::Response event_data =
      event->data(web_context()->environment_settings());
  EXPECT_TRUE(event_data.IsType<scoped_refptr<Blob>>());
  EXPECT_EQ(message_string.size(),
            event_data.AsType<scoped_refptr<Blob>>()->size());
  EXPECT_EQ("ConstructorWithBlobMessageData",
            std::string(reinterpret_cast<const char*>(
                            event_data.AsType<scoped_refptr<Blob>>()->data()),
                        static_cast<size_t>(
                            event_data.AsType<scoped_refptr<Blob>>()->size())));
  EXPECT_TRUE(event_data.AsType<scoped_refptr<Blob>>()->type().empty());
}

TEST_P(MessageEventTestWithJavaScript, ConstructorWithArrayBuffer) {
  std::string message_string("ConstructorWithArrayBufferMessageData");
  scoped_refptr<net::IOBufferWithSize> data =
      base::MakeRefCounted<net::IOBufferWithSize>(message_string.size());
  memcpy(data->data(), message_string.c_str(), message_string.size());
  scoped_refptr<MessageEvent> event = new MessageEvent(
      base::Tokens::message(), MessageEvent::kArrayBuffer, data);

  EXPECT_EQ("message", event->type());
  EXPECT_EQ(NULL, event->target().get());
  EXPECT_EQ(NULL, event->current_target().get());
  EXPECT_EQ(Event::kNone, event->event_phase());
  EXPECT_FALSE(event->bubbles());
  EXPECT_FALSE(event->cancelable());
  EXPECT_FALSE(event->default_prevented());
  EXPECT_FALSE(event->IsBeingDispatched());
  EXPECT_FALSE(event->propagation_stopped());
  EXPECT_FALSE(event->immediate_propagation_stopped());
  MessageEvent::Response event_data =
      event->data(web_context()->environment_settings());
  EXPECT_TRUE(event_data.IsType<script::Handle<script::ArrayBuffer>>());
  EXPECT_EQ(
      message_string.size(),
      event_data.AsType<script::Handle<script::ArrayBuffer>>()->ByteLength());
  EXPECT_EQ(
      "ConstructorWithArrayBufferMessageData",
      std::string(
          reinterpret_cast<const char*>(
              event_data.AsType<script::Handle<script::ArrayBuffer>>()->Data()),
          static_cast<size_t>(
              event_data.AsType<script::Handle<script::ArrayBuffer>>()
                  ->ByteLength())));
}

TEST_P(MessageEventTestWithJavaScript, ConstructorWithAny) {
  base::Optional<script::ValueHandleHolder::Reference> reference;
  EvaluateScript("'ConstructorWithAnyMessageData'", &reference);
  std::unique_ptr<script::DataBuffer> data(
      script::SerializeScriptValue(reference->referenced_value()));
  EXPECT_NE(nullptr, data.get());
  EXPECT_NE(nullptr, data->ptr);
  EXPECT_GT(data->size, 0U);
  scoped_refptr<MessageEvent> event =
      new MessageEvent(base::Tokens::message(), std::move(data));

  EXPECT_EQ("message", event->type());
  EXPECT_EQ(NULL, event->target().get());
  EXPECT_EQ(NULL, event->current_target().get());
  EXPECT_EQ(Event::kNone, event->event_phase());
  EXPECT_FALSE(event->bubbles());
  EXPECT_FALSE(event->cancelable());
  EXPECT_FALSE(event->default_prevented());
  EXPECT_FALSE(event->IsBeingDispatched());
  EXPECT_FALSE(event->propagation_stopped());
  EXPECT_FALSE(event->immediate_propagation_stopped());
  MessageEvent::Response event_data =
      event->data(web_context()->environment_settings());
  EXPECT_TRUE(event_data.IsType<script::Handle<script::ValueHandle>>());
  EXPECT_FALSE(
      event_data.AsType<script::Handle<script::ValueHandle>>().IsEmpty());
  auto script_value =
      event_data.AsType<script::Handle<script::ValueHandle>>().GetScriptValue();
  auto* isolate = script::GetIsolate(*script_value);
  script::v8c::EntryScope entry_scope(isolate);
  v8::Local<v8::Value> v8_value = script::GetV8Value(*script_value);
  std::string actual =
      *(v8::String::Utf8Value(isolate, v8_value.As<v8::String>()));
  EXPECT_EQ("ConstructorWithAnyMessageData", actual);
}

TEST_P(MessageEventTestWithJavaScript,
       ConstructorWithEventTypeAndDefaultInitDict) {
  MessageEventInit init;
  scoped_refptr<MessageEvent> event = new MessageEvent("mytestevent", init);

  EXPECT_EQ("mytestevent", event->type());
  EXPECT_EQ(nullptr, event->target().get());
  EXPECT_EQ(nullptr, event->current_target().get());
  EXPECT_EQ(Event::kNone, event->event_phase());
  EXPECT_FALSE(event->bubbles());
  EXPECT_FALSE(event->cancelable());
  EXPECT_FALSE(event->default_prevented());
  EXPECT_FALSE(event->IsBeingDispatched());
  EXPECT_FALSE(event->propagation_stopped());
  EXPECT_FALSE(event->immediate_propagation_stopped());

  MessageEvent::Response event_data =
      event->data(web_context()->environment_settings());
  EXPECT_TRUE(event_data.IsType<script::Handle<script::ValueHandle>>());
  EXPECT_TRUE(
      event_data.AsType<script::Handle<script::ValueHandle>>().IsEmpty());

  EXPECT_TRUE(event->origin().empty());
  EXPECT_TRUE(event->last_event_id().empty());
  EXPECT_EQ(nullptr, event->source().get());
  EXPECT_TRUE(event->ports().empty());
}

TEST_P(MessageEventTestWithJavaScript, ConstructorWithEventTypeAndInitDict) {
  MessageEventInit init;
  base::Optional<script::ValueHandleHolder::Reference> reference;
  EvaluateScript("'data_value'", &reference);
  init.set_data(&(reference->referenced_value()));
  init.set_origin("OriginString");
  init.set_last_event_id("lastEventIdString");
  init.set_source(web_context()->GetWindowOrWorkerGlobalScope());
  scoped_refptr<MessageEvent> event = new MessageEvent("mytestevent", init);

  EXPECT_EQ("mytestevent", event->type());
  EXPECT_EQ(nullptr, event->target().get());
  EXPECT_EQ(nullptr, event->current_target().get());
  EXPECT_EQ(Event::kNone, event->event_phase());
  EXPECT_FALSE(event->bubbles());
  EXPECT_FALSE(event->cancelable());
  EXPECT_FALSE(event->default_prevented());
  EXPECT_FALSE(event->IsBeingDispatched());
  EXPECT_FALSE(event->propagation_stopped());
  EXPECT_FALSE(event->immediate_propagation_stopped());

  MessageEvent::Response event_data =
      event->data(web_context()->environment_settings());
  EXPECT_TRUE(event_data.IsType<script::Handle<script::ValueHandle>>());
  auto script_value =
      event_data.AsType<script::Handle<script::ValueHandle>>().GetScriptValue();
  auto* isolate = script::GetIsolate(*script_value);
  script::v8c::EntryScope entry_scope(isolate);
  v8::Local<v8::Value> v8_value = script::GetV8Value(*script_value);
  std::string actual =
      *(v8::String::Utf8Value(isolate, v8_value.As<v8::String>()));
  EXPECT_EQ("data_value", actual);

  EXPECT_EQ("OriginString", event->origin());
  EXPECT_EQ("lastEventIdString", event->last_event_id());
  EXPECT_EQ(web_context()->GetWindowOrWorkerGlobalScope(),
            event->source().get());
  EXPECT_TRUE(event->ports().empty());
}

TEST_P(MessageEventTestWithJavaScript,
       ConstructorWithEventTypeAndMessageEventInitDict) {
  std::string result;
  EXPECT_TRUE(
      EvaluateScript("var event = new MessageEvent('dog', "
                     "    {cancelable: true, "
                     "     origin: 'OriginValue',"
                     "     lastEventId: 'LastEventIdValue',"
                     "     source: this,"
                     "     data: {value: 'data_value'},"
                     "    }"
                     ");"
                     "if (event.type == 'dog' &&"
                     "    event.bubbles == false &&"
                     "    event.cancelable == true &&"
                     "    event.origin == 'OriginValue' &&"
                     "    event.lastEventId == 'LastEventIdValue' &&"
                     "    event.source == this &&"
                     "    event.ports.length == 0 &&"
                     "    event.data.value == 'data_value') "
                     "    event.data.value;",
                     &result))
      << "Failed to evaluate script.";
  EXPECT_EQ("data_value", result);
}

INSTANTIATE_TEST_CASE_P(
    MessageEventTestsWithJavaScript, MessageEventTestWithJavaScript,
    ::testing::ValuesIn(testing::TestWebWithJavaScript::GetWebTypes()),
    testing::TestWebWithJavaScript::GetTypeName);

}  // namespace web
}  // namespace cobalt
