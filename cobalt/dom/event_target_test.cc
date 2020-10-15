// Copyright 2015 The Cobalt Authors. All Rights Reserved.
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

#include "cobalt/dom/event_target.h"

#include <memory>

#include "cobalt/base/polymorphic_downcast.h"
#include "cobalt/dom/dom_exception.h"
#include "cobalt/dom/dom_settings.h"
#include "cobalt/dom/global_stats.h"
#include "cobalt/dom/testing/mock_event_listener.h"
#include "cobalt/script/testing/fake_script_value.h"
#include "cobalt/script/testing/mock_exception_state.h"
#include "cobalt/test/mock_debugger_hooks.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cobalt {
namespace dom {
namespace {

using script::testing::FakeScriptValue;
using script::testing::MockExceptionState;
using ::testing::_;
using ::testing::AllOf;
using ::testing::DoAll;
using ::testing::Eq;
using ::testing::InSequence;
using ::testing::Invoke;
using ::testing::InvokeWithoutArgs;
using testing::MockEventListener;
using ::testing::Pointee;
using ::testing::Property;
using ::testing::SaveArg;
using ::testing::StrictMock;

constexpr auto kRecurring = base::DebuggerHooks::AsyncTaskFrequency::kRecurring;

class EventTargetTest : public ::testing::Test {
 protected:
  EventTargetTest()
      : environment_settings_(0, nullptr, nullptr, nullptr, nullptr, nullptr,
                              nullptr, nullptr, nullptr, debugger_hooks_,
                              nullptr, DOMSettings::Options()) {
    EXPECT_TRUE(GlobalStats::GetInstance()->CheckNoLeaks());
  }
  ~EventTargetTest() override {
    EXPECT_TRUE(GlobalStats::GetInstance()->CheckNoLeaks());
  }

  StrictMock<test::MockDebuggerHooks> debugger_hooks_;
  DOMSettings environment_settings_;
};

base::Optional<bool> DispatchEventOnCurrentTarget(
    const scoped_refptr<script::Wrappable>, const scoped_refptr<Event>& event,
    bool*) {
  StrictMock<MockExceptionState> exception_state;
  scoped_refptr<script::ScriptException> exception;

  EXPECT_TRUE(event->IsBeingDispatched());

  EXPECT_CALL(exception_state, SetException(_))
      .WillOnce(SaveArg<0>(&exception));
  event->current_target()->DispatchEvent(event, &exception_state);

  EXPECT_TRUE(exception);
  if (!exception) {
    EXPECT_EQ(
        DOMException::kInvalidStateErr,
        base::polymorphic_downcast<DOMException*>(exception.get())->code());
  }
  return base::nullopt;
}

TEST_F(EventTargetTest, SingleEventListenerFired) {
  StrictMock<MockExceptionState> exception_state;
  scoped_refptr<EventTarget> event_target =
      new EventTarget(&environment_settings_);
  scoped_refptr<Event> event = new Event(base::Token("fired"));
  std::unique_ptr<MockEventListener> event_listener =
      MockEventListener::Create();

  const void* async_task;
  EXPECT_CALL(debugger_hooks_, AsyncTaskScheduled(_, "fired", kRecurring))
      .WillOnce(SaveArg<0>(&async_task));
  event_target->AddEventListener(
      "fired", FakeScriptValue<EventListener>(event_listener.get()), false);

  event_listener->ExpectHandleEventCall(event, event_target);
  EXPECT_CALL(debugger_hooks_, AsyncTaskStarted(async_task));
  EXPECT_CALL(debugger_hooks_, AsyncTaskFinished(async_task));
  EXPECT_TRUE(event_target->DispatchEvent(event, &exception_state));
}

TEST_F(EventTargetTest, SingleEventListenerNotFired) {
  StrictMock<MockExceptionState> exception_state;
  scoped_refptr<EventTarget> event_target =
      new EventTarget(&environment_settings_);
  scoped_refptr<Event> event = new Event(base::Token("fired"));
  std::unique_ptr<MockEventListener> event_listener =
      MockEventListener::Create();

  EXPECT_CALL(debugger_hooks_, AsyncTaskScheduled(_, "notfired", kRecurring));
  event_target->AddEventListener(
      "notfired", FakeScriptValue<EventListener>(event_listener.get()), false);

  event_listener->ExpectNoHandleEventCall();
  EXPECT_TRUE(event_target->DispatchEvent(event, &exception_state));
}

// Test if multiple event listeners of different event types can be added and
// fired properly.
TEST_F(EventTargetTest, MultipleEventListeners) {
  StrictMock<MockExceptionState> exception_state;
  scoped_refptr<EventTarget> event_target =
      new EventTarget(&environment_settings_);
  scoped_refptr<Event> event = new Event(base::Token("fired"));
  std::unique_ptr<MockEventListener> event_listenerfired_1 =
      MockEventListener::Create();
  std::unique_ptr<MockEventListener> event_listenerfired_2 =
      MockEventListener::Create();
  std::unique_ptr<MockEventListener> event_listenernot_fired =
      MockEventListener::Create();

  InSequence in_sequence;

  const void* async_task_1;
  const void* async_task_2;
  const void* async_task_not_fired;
  EXPECT_CALL(debugger_hooks_, AsyncTaskScheduled(_, "fired", kRecurring))
      .WillOnce(SaveArg<0>(&async_task_1));
  EXPECT_CALL(debugger_hooks_, AsyncTaskScheduled(_, "notfired", kRecurring))
      .WillOnce(SaveArg<0>(&async_task_not_fired));
  EXPECT_CALL(debugger_hooks_, AsyncTaskScheduled(_, "fired", kRecurring))
      .WillOnce(SaveArg<0>(&async_task_2));

  event_target->AddEventListener(
      "fired", FakeScriptValue<EventListener>(event_listenerfired_1.get()),
      false);
  event_target->AddEventListener(
      "notfired", FakeScriptValue<EventListener>(event_listenernot_fired.get()),
      false);
  event_target->AddEventListener(
      "fired", FakeScriptValue<EventListener>(event_listenerfired_2.get()),
      true);

  EXPECT_CALL(debugger_hooks_, AsyncTaskStarted(async_task_1));
  event_listenerfired_1->ExpectHandleEventCall(event, event_target);
  EXPECT_CALL(debugger_hooks_, AsyncTaskFinished(async_task_1));

  EXPECT_CALL(debugger_hooks_, AsyncTaskStarted(async_task_2));
  event_listenerfired_2->ExpectHandleEventCall(event, event_target);
  EXPECT_CALL(debugger_hooks_, AsyncTaskFinished(async_task_2));

  event_listenernot_fired->ExpectNoHandleEventCall();

  EXPECT_TRUE(event_target->DispatchEvent(event, &exception_state));
}

// Test if event listener can be added and later removed.
TEST_F(EventTargetTest, AddRemoveEventListener) {
  StrictMock<MockExceptionState> exception_state;
  scoped_refptr<EventTarget> event_target =
      new EventTarget(&environment_settings_);
  scoped_refptr<Event> event = new Event(base::Token("fired"));
  std::unique_ptr<MockEventListener> event_listener =
      MockEventListener::Create();

  const void* async_task_1;
  EXPECT_CALL(debugger_hooks_, AsyncTaskScheduled(_, "fired", kRecurring))
      .WillOnce(SaveArg<0>(&async_task_1));
  event_target->AddEventListener(
      "fired", FakeScriptValue<EventListener>(event_listener.get()), false);
  EXPECT_CALL(debugger_hooks_, AsyncTaskStarted(async_task_1));
  event_listener->ExpectHandleEventCall(event, event_target);
  EXPECT_CALL(debugger_hooks_, AsyncTaskFinished(async_task_1));
  EXPECT_TRUE(event_target->DispatchEvent(event, &exception_state));

  EXPECT_CALL(debugger_hooks_, AsyncTaskCanceled(async_task_1));
  event_target->RemoveEventListener(
      "fired", FakeScriptValue<EventListener>(event_listener.get()), false);
  event_listener->ExpectNoHandleEventCall();
  EXPECT_TRUE(event_target->DispatchEvent(event, &exception_state));

  const void* async_task_2;
  EXPECT_CALL(debugger_hooks_, AsyncTaskScheduled(_, "fired", kRecurring))
      .WillOnce(SaveArg<0>(&async_task_2));
  event_target->AddEventListener(
      "fired", FakeScriptValue<EventListener>(event_listener.get()), false);
  EXPECT_CALL(debugger_hooks_, AsyncTaskStarted(async_task_2));
  event_listener->ExpectHandleEventCall(event, event_target);
  EXPECT_CALL(debugger_hooks_, AsyncTaskFinished(async_task_2));
  EXPECT_TRUE(event_target->DispatchEvent(event, &exception_state));
}

// Test if attribute event listener works.
TEST_F(EventTargetTest, AttributeListener) {
  StrictMock<MockExceptionState> exception_state;
  scoped_refptr<EventTarget> event_target =
      new EventTarget(&environment_settings_);
  scoped_refptr<Event> event = new Event(base::Token("fired"));
  std::unique_ptr<MockEventListener> non_attribute_event_listener =
      MockEventListener::Create();
  std::unique_ptr<MockEventListener> attribute_event_listener1 =
      MockEventListener::Create();
  std::unique_ptr<MockEventListener> attribute_event_listener2 =
      MockEventListener::Create();

  const void* non_attribute_async_task;
  EXPECT_CALL(debugger_hooks_, AsyncTaskScheduled(_, "fired", kRecurring))
      .WillOnce(SaveArg<0>(&non_attribute_async_task));
  event_target->AddEventListener(
      "fired",
      FakeScriptValue<EventListener>(non_attribute_event_listener.get()),
      false);
  const void* async_task_1;
  EXPECT_CALL(debugger_hooks_, AsyncTaskScheduled(_, "fired", kRecurring))
      .WillOnce(SaveArg<0>(&async_task_1));
  event_target->SetAttributeEventListener(
      base::Token("fired"),
      FakeScriptValue<EventListener>(attribute_event_listener1.get()));
  EXPECT_CALL(debugger_hooks_, AsyncTaskStarted(non_attribute_async_task));
  non_attribute_event_listener->ExpectHandleEventCall(event, event_target);
  EXPECT_CALL(debugger_hooks_, AsyncTaskFinished(non_attribute_async_task));
  EXPECT_CALL(debugger_hooks_, AsyncTaskStarted(async_task_1));
  attribute_event_listener1->ExpectHandleEventCall(event, event_target);
  EXPECT_CALL(debugger_hooks_, AsyncTaskFinished(async_task_1));
  EXPECT_TRUE(event_target->DispatchEvent(event, &exception_state));

  const void* async_task_2;
  EXPECT_CALL(debugger_hooks_, AsyncTaskCanceled(async_task_1));
  EXPECT_CALL(debugger_hooks_, AsyncTaskScheduled(_, "fired", kRecurring))
      .WillOnce(SaveArg<0>(&async_task_2));
  event_target->SetAttributeEventListener(
      base::Token("fired"),
      FakeScriptValue<EventListener>(attribute_event_listener2.get()));
  EXPECT_CALL(debugger_hooks_, AsyncTaskStarted(non_attribute_async_task));
  non_attribute_event_listener->ExpectHandleEventCall(event, event_target);
  EXPECT_CALL(debugger_hooks_, AsyncTaskFinished(non_attribute_async_task));
  attribute_event_listener1->ExpectNoHandleEventCall();
  EXPECT_CALL(debugger_hooks_, AsyncTaskStarted(async_task_2));
  attribute_event_listener2->ExpectHandleEventCall(event, event_target);
  EXPECT_CALL(debugger_hooks_, AsyncTaskFinished(async_task_2));
  EXPECT_TRUE(event_target->DispatchEvent(event, &exception_state));

  EXPECT_CALL(debugger_hooks_, AsyncTaskCanceled(async_task_2));
  event_target->SetAttributeEventListener(base::Token("fired"),
                                          FakeScriptValue<EventListener>(NULL));
  EXPECT_CALL(debugger_hooks_, AsyncTaskStarted(non_attribute_async_task));
  non_attribute_event_listener->ExpectHandleEventCall(event, event_target);
  EXPECT_CALL(debugger_hooks_, AsyncTaskFinished(non_attribute_async_task));
  attribute_event_listener1->ExpectNoHandleEventCall();
  attribute_event_listener2->ExpectNoHandleEventCall();
  EXPECT_TRUE(event_target->DispatchEvent(event, &exception_state));
}

// Test if one event listener can be used by multiple events.
TEST_F(EventTargetTest, EventListenerReuse) {
  StrictMock<MockExceptionState> exception_state;
  scoped_refptr<EventTarget> event_target =
      new EventTarget(&environment_settings_);
  scoped_refptr<Event> event_1 = new Event(base::Token("fired_1"));
  scoped_refptr<Event> event_2 = new Event(base::Token("fired_2"));
  std::unique_ptr<MockEventListener> event_listener =
      MockEventListener::Create();

  const void* async_task_1;
  EXPECT_CALL(debugger_hooks_, AsyncTaskScheduled(_, "fired_1", kRecurring))
      .WillOnce(SaveArg<0>(&async_task_1));
  event_target->AddEventListener(
      "fired_1", FakeScriptValue<EventListener>(event_listener.get()), false);
  const void* async_task_2;
  EXPECT_CALL(debugger_hooks_, AsyncTaskScheduled(_, "fired_2", kRecurring))
      .WillOnce(SaveArg<0>(&async_task_2));
  event_target->AddEventListener(
      "fired_2", FakeScriptValue<EventListener>(event_listener.get()), false);
  EXPECT_CALL(debugger_hooks_, AsyncTaskStarted(async_task_1));
  event_listener->ExpectHandleEventCall(event_1, event_target);
  EXPECT_CALL(debugger_hooks_, AsyncTaskFinished(async_task_1));
  EXPECT_TRUE(event_target->DispatchEvent(event_1, &exception_state));
  EXPECT_CALL(debugger_hooks_, AsyncTaskStarted(async_task_2));
  event_listener->ExpectHandleEventCall(event_2, event_target);
  EXPECT_CALL(debugger_hooks_, AsyncTaskFinished(async_task_2));
  EXPECT_TRUE(event_target->DispatchEvent(event_2, &exception_state));

  EXPECT_CALL(debugger_hooks_, AsyncTaskCanceled(async_task_2));
  event_target->RemoveEventListener(
      "fired_2", FakeScriptValue<EventListener>(event_listener.get()), false);
  EXPECT_CALL(debugger_hooks_, AsyncTaskStarted(async_task_1));
  event_listener->ExpectHandleEventCall(event_1, event_target);
  EXPECT_CALL(debugger_hooks_, AsyncTaskFinished(async_task_1));
  EXPECT_TRUE(event_target->DispatchEvent(event_1, &exception_state));
  EXPECT_TRUE(event_target->DispatchEvent(event_2, &exception_state));

  // The capture flag is not the same so the event will not be removed.
  event_target->RemoveEventListener(
      "fired_1", FakeScriptValue<EventListener>(event_listener.get()), true);
  EXPECT_CALL(debugger_hooks_, AsyncTaskStarted(async_task_1));
  event_listener->ExpectHandleEventCall(event_1, event_target);
  EXPECT_CALL(debugger_hooks_, AsyncTaskFinished(async_task_1));
  EXPECT_TRUE(event_target->DispatchEvent(event_1, &exception_state));
  EXPECT_TRUE(event_target->DispatchEvent(event_2, &exception_state));

  EXPECT_CALL(debugger_hooks_, AsyncTaskCanceled(async_task_1));
  event_target->RemoveEventListener(
      "fired_1", FakeScriptValue<EventListener>(event_listener.get()), false);
  event_listener->ExpectNoHandleEventCall();
  EXPECT_TRUE(event_target->DispatchEvent(event_1, &exception_state));
  EXPECT_TRUE(event_target->DispatchEvent(event_2, &exception_state));
}

TEST_F(EventTargetTest, StopPropagation) {
  StrictMock<MockExceptionState> exception_state;
  scoped_refptr<EventTarget> event_target =
      new EventTarget(&environment_settings_);
  scoped_refptr<Event> event = new Event(base::Token("fired"));
  std::unique_ptr<MockEventListener> event_listenerfired_1 =
      MockEventListener::Create();
  std::unique_ptr<MockEventListener> event_listenerfired_2 =
      MockEventListener::Create();

  InSequence in_sequence;

  const void* async_task_1;
  const void* async_task_2;
  EXPECT_CALL(debugger_hooks_, AsyncTaskScheduled(_, "fired", kRecurring))
      .WillOnce(SaveArg<0>(&async_task_1));
  EXPECT_CALL(debugger_hooks_, AsyncTaskScheduled(_, "fired", kRecurring))
      .WillOnce(SaveArg<0>(&async_task_2));
  event_target->AddEventListener(
      "fired", FakeScriptValue<EventListener>(event_listenerfired_1.get()),
      false);
  event_target->AddEventListener(
      "fired", FakeScriptValue<EventListener>(event_listenerfired_2.get()),
      true);

  EXPECT_CALL(debugger_hooks_, AsyncTaskStarted(async_task_1));
  event_listenerfired_1->ExpectHandleEventCall(
      event, event_target, &MockEventListener::StopPropagation);
  EXPECT_CALL(debugger_hooks_, AsyncTaskFinished(async_task_1));
  EXPECT_CALL(debugger_hooks_, AsyncTaskStarted(async_task_2));
  event_listenerfired_2->ExpectHandleEventCall(event, event_target);
  EXPECT_CALL(debugger_hooks_, AsyncTaskFinished(async_task_2));
  EXPECT_TRUE(event_target->DispatchEvent(event, &exception_state));
}

TEST_F(EventTargetTest, StopImmediatePropagation) {
  StrictMock<MockExceptionState> exception_state;
  scoped_refptr<EventTarget> event_target =
      new EventTarget(&environment_settings_);
  scoped_refptr<Event> event = new Event(base::Token("fired"));
  std::unique_ptr<MockEventListener> event_listenerfired_1 =
      MockEventListener::Create();
  std::unique_ptr<MockEventListener> event_listenerfired_2 =
      MockEventListener::Create();

  const void* async_task_1;
  EXPECT_CALL(debugger_hooks_, AsyncTaskScheduled(_, "fired", kRecurring))
      .WillOnce(SaveArg<0>(&async_task_1));
  event_target->AddEventListener(
      "fired", FakeScriptValue<EventListener>(event_listenerfired_1.get()),
      false);
  const void* async_task_2;
  EXPECT_CALL(debugger_hooks_, AsyncTaskScheduled(_, "fired", kRecurring))
      .WillOnce(SaveArg<0>(&async_task_2));
  event_target->AddEventListener(
      "fired", FakeScriptValue<EventListener>(event_listenerfired_2.get()),
      true);

  EXPECT_CALL(debugger_hooks_, AsyncTaskStarted(async_task_1));
  event_listenerfired_1->ExpectHandleEventCall(
      event, event_target, &MockEventListener::StopImmediatePropagation);
  EXPECT_CALL(debugger_hooks_, AsyncTaskFinished(async_task_1));
  event_listenerfired_2->ExpectNoHandleEventCall();
  EXPECT_TRUE(event_target->DispatchEvent(event, &exception_state));
}

TEST_F(EventTargetTest, PreventDefault) {
  StrictMock<MockExceptionState> exception_state;
  scoped_refptr<Event> event;
  scoped_refptr<EventTarget> event_target =
      new EventTarget(&environment_settings_);
  std::unique_ptr<MockEventListener> event_listenerfired =
      MockEventListener::Create();

  const void* async_task;
  EXPECT_CALL(debugger_hooks_, AsyncTaskScheduled(_, "fired", kRecurring))
      .WillOnce(SaveArg<0>(&async_task));
  event_target->AddEventListener(
      "fired", FakeScriptValue<EventListener>(event_listenerfired.get()),
      false);
  event = new Event(base::Token("fired"), Event::kNotBubbles,
                    Event::kNotCancelable);
  EXPECT_CALL(debugger_hooks_, AsyncTaskStarted(async_task));
  event_listenerfired->ExpectHandleEventCall(
      event, event_target, &MockEventListener::PreventDefault);
  EXPECT_CALL(debugger_hooks_, AsyncTaskFinished(async_task));
  EXPECT_TRUE(event_target->DispatchEvent(event, &exception_state));

  event =
      new Event(base::Token("fired"), Event::kNotBubbles, Event::kCancelable);
  EXPECT_CALL(debugger_hooks_, AsyncTaskStarted(async_task));
  event_listenerfired->ExpectHandleEventCall(
      event, event_target, &MockEventListener::PreventDefault);
  EXPECT_CALL(debugger_hooks_, AsyncTaskFinished(async_task));
  EXPECT_FALSE(event_target->DispatchEvent(event, &exception_state));
}

TEST_F(EventTargetTest, RaiseException) {
  StrictMock<MockExceptionState> exception_state;
  scoped_refptr<script::ScriptException> exception;
  scoped_refptr<EventTarget> event_target =
      new EventTarget(&environment_settings_);
  scoped_refptr<Event> event;
  std::unique_ptr<MockEventListener> event_listener =
      MockEventListener::Create();

  EXPECT_CALL(exception_state, SetException(_))
      .WillOnce(SaveArg<0>(&exception));
  // Dispatch a NULL event.
  event_target->DispatchEvent(NULL, &exception_state);
  ASSERT_TRUE(exception);
  EXPECT_EQ(DOMException::kInvalidStateErr,
            base::polymorphic_downcast<DOMException*>(exception.get())->code());
  exception = NULL;

  EXPECT_CALL(exception_state, SetException(_))
      .WillOnce(SaveArg<0>(&exception));
  // Dispatch an uninitialized event.
  event_target->DispatchEvent(new Event(Event::Uninitialized),
                              &exception_state);
  ASSERT_TRUE(exception);
  EXPECT_EQ(DOMException::kInvalidStateErr,
            base::polymorphic_downcast<DOMException*>(exception.get())->code());
  exception = NULL;

  const void* async_task;
  EXPECT_CALL(debugger_hooks_, AsyncTaskScheduled(_, "fired", kRecurring))
      .WillOnce(SaveArg<0>(&async_task));
  event_target->AddEventListener(
      "fired", FakeScriptValue<EventListener>(event_listener.get()), false);
  event = new Event(base::Token("fired"), Event::kNotBubbles,
                    Event::kNotCancelable);
  // Dispatch event again when it is being dispatched.
  EXPECT_CALL(debugger_hooks_, AsyncTaskStarted(async_task));
  EXPECT_CALL(*event_listener, HandleEvent(_, _, _))
      .WillOnce(Invoke(DispatchEventOnCurrentTarget));
  EXPECT_CALL(debugger_hooks_, AsyncTaskFinished(async_task));
  EXPECT_TRUE(event_target->DispatchEvent(event, &exception_state));
}

TEST_F(EventTargetTest, AddSameListenerMultipleTimes) {
  StrictMock<MockExceptionState> exception_state;
  scoped_refptr<EventTarget> event_target =
      new EventTarget(&environment_settings_);
  scoped_refptr<Event> event = new Event(base::Token("fired"));
  std::unique_ptr<MockEventListener> event_listener =
      MockEventListener::Create();
  FakeScriptValue<EventListener> script_object(event_listener.get());

  InSequence in_sequence;

  // The same listener should only get added once.
  const void* async_task;
  EXPECT_CALL(debugger_hooks_, AsyncTaskScheduled(_, "fired", kRecurring))
      .WillOnce(SaveArg<0>(&async_task));
  event_target->AddEventListener("fired", script_object, false);
  event_target->AddEventListener("fired", script_object, false);
  event_target->AddEventListener("fired", script_object, false);

  EXPECT_CALL(debugger_hooks_, AsyncTaskStarted(async_task));
  event_listener->ExpectHandleEventCall(event, event_target);
  EXPECT_CALL(debugger_hooks_, AsyncTaskFinished(async_task));
  EXPECT_TRUE(event_target->DispatchEvent(event, &exception_state));
}

TEST_F(EventTargetTest, AddSameAttributeListenerMultipleTimes) {
  StrictMock<MockExceptionState> exception_state;
  scoped_refptr<EventTarget> event_target =
      new EventTarget(&environment_settings_);
  scoped_refptr<Event> event = new Event(base::Token("fired"));
  std::unique_ptr<MockEventListener> event_listener =
      MockEventListener::Create();
  FakeScriptValue<EventListener> script_object(event_listener.get());

  InSequence in_sequence;

  // The same listener should only get added once.
  const void* async_task_1;
  EXPECT_CALL(debugger_hooks_, AsyncTaskScheduled(_, "fired", kRecurring))
      .WillOnce(SaveArg<0>(&async_task_1));
  event_target->SetAttributeEventListener(base::Token("fired"), script_object);

  const void* async_task_2;
  EXPECT_CALL(debugger_hooks_, AsyncTaskCanceled(async_task_1));
  EXPECT_CALL(debugger_hooks_, AsyncTaskScheduled(_, "fired", kRecurring))
      .WillOnce(SaveArg<0>(&async_task_2));
  event_target->SetAttributeEventListener(base::Token("fired"), script_object);

  const void* async_task_3;
  EXPECT_CALL(debugger_hooks_, AsyncTaskCanceled(async_task_2));
  EXPECT_CALL(debugger_hooks_, AsyncTaskScheduled(_, "fired", kRecurring))
      .WillOnce(SaveArg<0>(&async_task_3));
  event_target->SetAttributeEventListener(base::Token("fired"), script_object);

  EXPECT_CALL(debugger_hooks_, AsyncTaskStarted(async_task_3));
  event_listener->ExpectHandleEventCall(event, event_target);
  EXPECT_CALL(debugger_hooks_, AsyncTaskFinished(async_task_3));
  EXPECT_TRUE(event_target->DispatchEvent(event, &exception_state));
}

TEST_F(EventTargetTest, SameEventListenerAsAttribute) {
  StrictMock<MockExceptionState> exception_state;
  scoped_refptr<EventTarget> event_target =
      new EventTarget(&environment_settings_);
  scoped_refptr<Event> event = new Event(base::Token("fired"));
  std::unique_ptr<MockEventListener> event_listener =
      MockEventListener::Create();
  FakeScriptValue<EventListener> script_object(event_listener.get());

  InSequence in_sequence;

  // The same script object can be registered as both an attribute and
  // non-attribute listener. Both should be fired.
  const void* async_task_1;
  EXPECT_CALL(debugger_hooks_, AsyncTaskScheduled(_, "fired", kRecurring))
      .WillOnce(SaveArg<0>(&async_task_1));
  event_target->AddEventListener("fired", script_object, false);

  const void* async_task_2;
  EXPECT_CALL(debugger_hooks_, AsyncTaskScheduled(_, "fired", kRecurring))
      .WillOnce(SaveArg<0>(&async_task_2));
  event_target->SetAttributeEventListener(base::Token("fired"), script_object);

  EXPECT_CALL(debugger_hooks_, AsyncTaskStarted(async_task_1));
  event_listener->ExpectHandleEventCall(event, event_target);
  EXPECT_CALL(debugger_hooks_, AsyncTaskFinished(async_task_1));
  EXPECT_CALL(debugger_hooks_, AsyncTaskStarted(async_task_2));
  event_listener->ExpectHandleEventCall(event, event_target);
  EXPECT_CALL(debugger_hooks_, AsyncTaskFinished(async_task_2));
  EXPECT_TRUE(event_target->DispatchEvent(event, &exception_state));
}

}  // namespace
}  // namespace dom
}  // namespace cobalt
