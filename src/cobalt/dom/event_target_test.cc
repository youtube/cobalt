/*
 * Copyright 2015 Google Inc. All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "cobalt/dom/event_target.h"

#include "cobalt/base/polymorphic_downcast.h"
#include "cobalt/dom/dom_exception.h"
#include "cobalt/dom/testing/mock_event_listener.h"
#include "cobalt/script/testing/mock_exception_state.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cobalt {
namespace dom {
namespace {

using ::testing::AllOf;
using ::testing::DoAll;
using ::testing::Eq;
using ::testing::InSequence;
using ::testing::Invoke;
using ::testing::InvokeWithoutArgs;
using ::testing::Pointee;
using ::testing::Property;
using ::testing::SaveArg;
using ::testing::StrictMock;
using ::testing::_;
using script::testing::MockExceptionState;
using testing::FakeScriptObject;
using testing::MockEventListener;

base::optional<bool> DispatchEventOnCurrentTarget(
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

TEST(EventTargetTest, SingleEventListenerFired) {
  StrictMock<MockExceptionState> exception_state;
  scoped_refptr<EventTarget> event_target = new EventTarget;
  scoped_refptr<Event> event = new Event(base::Token("fired"));
  scoped_ptr<MockEventListener> event_listener = MockEventListener::Create();

  event_listener->ExpectHandleEventCall(event, event_target);
  event_target->AddEventListener("fired",
                                 FakeScriptObject(event_listener.get()), false);
  EXPECT_TRUE(event_target->DispatchEvent(event, &exception_state));
}

TEST(EventTargetTest, SingleEventListenerNotFired) {
  StrictMock<MockExceptionState> exception_state;
  scoped_refptr<EventTarget> event_target = new EventTarget;
  scoped_refptr<Event> event = new Event(base::Token("fired"));
  scoped_ptr<MockEventListener> event_listener = MockEventListener::Create();

  event_listener->ExpectNoHandleEventCall();
  event_target->AddEventListener("notfired",
                                 FakeScriptObject(event_listener.get()), false);
  EXPECT_TRUE(event_target->DispatchEvent(event, &exception_state));
}

// Test if multiple event listeners of different event types can be added and
// fired properly.
TEST(EventTargetTest, MultipleEventListeners) {
  StrictMock<MockExceptionState> exception_state;
  scoped_refptr<EventTarget> event_target = new EventTarget;
  scoped_refptr<Event> event = new Event(base::Token("fired"));
  scoped_ptr<MockEventListener> event_listenerfired_1 =
      MockEventListener::Create();
  scoped_ptr<MockEventListener> event_listenerfired_2 =
      MockEventListener::Create();
  scoped_ptr<MockEventListener> event_listenernot_fired =
      MockEventListener::Create();

  InSequence in_sequence;
  event_listenerfired_1->ExpectHandleEventCall(event, event_target);
  event_listenerfired_2->ExpectHandleEventCall(event, event_target);
  event_listenernot_fired->ExpectNoHandleEventCall();

  event_target->AddEventListener(
      "fired", FakeScriptObject(event_listenerfired_1.get()), false);
  event_target->AddEventListener(
      "notfired", FakeScriptObject(event_listenernot_fired.get()), false);
  event_target->AddEventListener(
      "fired", FakeScriptObject(event_listenerfired_2.get()), true);
  EXPECT_TRUE(event_target->DispatchEvent(event, &exception_state));
}

// Test if event listener can be added and later removed.
TEST(EventTargetTest, AddRemoveEventListener) {
  StrictMock<MockExceptionState> exception_state;
  scoped_refptr<EventTarget> event_target = new EventTarget;
  scoped_refptr<Event> event = new Event(base::Token("fired"));
  scoped_ptr<MockEventListener> event_listener = MockEventListener::Create();

  event_listener->ExpectHandleEventCall(event, event_target);
  event_target->AddEventListener("fired",
                                 FakeScriptObject(event_listener.get()), false);
  EXPECT_TRUE(event_target->DispatchEvent(event, &exception_state));

  event_listener->ExpectNoHandleEventCall();
  event_target->RemoveEventListener(
      "fired", FakeScriptObject(event_listener.get()), false);
  EXPECT_TRUE(event_target->DispatchEvent(event, &exception_state));

  event_listener->ExpectHandleEventCall(event, event_target);
  event_target->AddEventListener("fired",
                                 FakeScriptObject(event_listener.get()), false);
  EXPECT_TRUE(event_target->DispatchEvent(event, &exception_state));
}

// Test if attribute event listener works.
TEST(EventTargetTest, AttributeListener) {
  StrictMock<MockExceptionState> exception_state;
  scoped_refptr<EventTarget> event_target = new EventTarget;
  scoped_refptr<Event> event = new Event(base::Token("fired"));
  scoped_ptr<MockEventListener> non_attribute_event_listener =
      MockEventListener::Create();
  scoped_ptr<MockEventListener> attribute_event_listener1 =
      MockEventListener::Create();
  scoped_ptr<MockEventListener> attribute_event_listener2 =
      MockEventListener::Create();

  event_target->AddEventListener(
      "fired", FakeScriptObject(non_attribute_event_listener.get()), false);

  non_attribute_event_listener->ExpectHandleEventCall(event, event_target);
  attribute_event_listener1->ExpectHandleEventCall(event, event_target);
  event_target->SetAttributeEventListener(
      base::Token("fired"), FakeScriptObject(attribute_event_listener1.get()));
  EXPECT_TRUE(event_target->DispatchEvent(event, &exception_state));

  non_attribute_event_listener->ExpectHandleEventCall(event, event_target);
  attribute_event_listener1->ExpectNoHandleEventCall();
  attribute_event_listener2->ExpectHandleEventCall(event, event_target);
  event_target->SetAttributeEventListener(
      base::Token("fired"), FakeScriptObject(attribute_event_listener2.get()));
  EXPECT_TRUE(event_target->DispatchEvent(event, &exception_state));

  non_attribute_event_listener->ExpectHandleEventCall(event, event_target);
  attribute_event_listener1->ExpectNoHandleEventCall();
  attribute_event_listener2->ExpectNoHandleEventCall();
  event_target->SetAttributeEventListener(base::Token("fired"),
                                          FakeScriptObject(NULL));
  EXPECT_TRUE(event_target->DispatchEvent(event, &exception_state));
}

// Test if one event listener can be used by multiple events.
TEST(EventTargetTest, EventListenerReuse) {
  StrictMock<MockExceptionState> exception_state;
  scoped_refptr<EventTarget> event_target = new EventTarget;
  scoped_refptr<Event> event_1 = new Event(base::Token("fired_1"));
  scoped_refptr<Event> event_2 = new Event(base::Token("fired_2"));
  scoped_ptr<MockEventListener> event_listener = MockEventListener::Create();

  event_listener->ExpectHandleEventCall(event_1, event_target);
  event_listener->ExpectHandleEventCall(event_2, event_target);
  event_target->AddEventListener("fired_1",
                                 FakeScriptObject(event_listener.get()), false);
  event_target->AddEventListener("fired_2",
                                 FakeScriptObject(event_listener.get()), false);
  EXPECT_TRUE(event_target->DispatchEvent(event_1, &exception_state));
  EXPECT_TRUE(event_target->DispatchEvent(event_2, &exception_state));

  event_listener->ExpectHandleEventCall(event_1, event_target);
  event_target->RemoveEventListener(
      "fired_2", FakeScriptObject(event_listener.get()), false);
  EXPECT_TRUE(event_target->DispatchEvent(event_1, &exception_state));
  EXPECT_TRUE(event_target->DispatchEvent(event_2, &exception_state));

  event_listener->ExpectHandleEventCall(event_1, event_target);
  // The capture flag is not the same so the event will not be removed.
  event_target->RemoveEventListener(
      "fired_1", FakeScriptObject(event_listener.get()), true);
  EXPECT_TRUE(event_target->DispatchEvent(event_1, &exception_state));
  EXPECT_TRUE(event_target->DispatchEvent(event_2, &exception_state));

  event_listener->ExpectNoHandleEventCall();
  event_target->RemoveEventListener(
      "fired_1", FakeScriptObject(event_listener.get()), false);
  EXPECT_TRUE(event_target->DispatchEvent(event_1, &exception_state));
  EXPECT_TRUE(event_target->DispatchEvent(event_2, &exception_state));
}

TEST(EventTargetTest, StopPropagation) {
  StrictMock<MockExceptionState> exception_state;
  scoped_refptr<EventTarget> event_target = new EventTarget;
  scoped_refptr<Event> event = new Event(base::Token("fired"));
  scoped_ptr<MockEventListener> event_listenerfired_1 =
      MockEventListener::Create();
  scoped_ptr<MockEventListener> event_listenerfired_2 =
      MockEventListener::Create();

  InSequence in_sequence;
  event_listenerfired_1->ExpectHandleEventCall(
      event, event_target, &MockEventListener::StopPropagation);
  event_listenerfired_2->ExpectHandleEventCall(event, event_target);

  event_target->AddEventListener(
      "fired", FakeScriptObject(event_listenerfired_1.get()), false);
  event_target->AddEventListener(
      "fired", FakeScriptObject(event_listenerfired_2.get()), true);
  EXPECT_TRUE(event_target->DispatchEvent(event, &exception_state));
}

TEST(EventTargetTest, StopImmediatePropagation) {
  StrictMock<MockExceptionState> exception_state;
  scoped_refptr<EventTarget> event_target = new EventTarget;
  scoped_refptr<Event> event = new Event(base::Token("fired"));
  scoped_ptr<MockEventListener> event_listenerfired_1 =
      MockEventListener::Create();
  scoped_ptr<MockEventListener> event_listenerfired_2 =
      MockEventListener::Create();

  event_listenerfired_1->ExpectHandleEventCall(
      event, event_target, &MockEventListener::StopImmediatePropagation);
  event_listenerfired_2->ExpectNoHandleEventCall();

  event_target->AddEventListener(
      "fired", FakeScriptObject(event_listenerfired_1.get()), false);
  event_target->AddEventListener(
      "fired", FakeScriptObject(event_listenerfired_2.get()), true);
  EXPECT_TRUE(event_target->DispatchEvent(event, &exception_state));
}

TEST(EventTargetTest, PreventDefault) {
  StrictMock<MockExceptionState> exception_state;
  scoped_refptr<Event> event;
  scoped_refptr<EventTarget> event_target = new EventTarget;
  scoped_ptr<MockEventListener> event_listenerfired =
      MockEventListener::Create();

  event_target->AddEventListener(
      "fired", FakeScriptObject(event_listenerfired.get()), false);
  event = new Event(base::Token("fired"), Event::kNotBubbles,
                    Event::kNotCancelable);
  event_listenerfired->ExpectHandleEventCall(
      event, event_target, &MockEventListener::PreventDefault);
  EXPECT_TRUE(event_target->DispatchEvent(event, &exception_state));

  event =
      new Event(base::Token("fired"), Event::kNotBubbles, Event::kCancelable);
  event_listenerfired->ExpectHandleEventCall(
      event, event_target, &MockEventListener::PreventDefault);
  EXPECT_FALSE(event_target->DispatchEvent(event, &exception_state));
}

TEST(EventTargetTest, RaiseException) {
  StrictMock<MockExceptionState> exception_state;
  scoped_refptr<script::ScriptException> exception;
  scoped_refptr<EventTarget> event_target = new EventTarget;
  scoped_refptr<Event> event;
  scoped_ptr<MockEventListener> event_listener = MockEventListener::Create();

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

  event_target->AddEventListener("fired",
                                 FakeScriptObject(event_listener.get()), false);
  event = new Event(base::Token("fired"), Event::kNotBubbles,
                    Event::kNotCancelable);
  // Dispatch event again when it is being dispatched.
  EXPECT_CALL(*event_listener, HandleEvent(_, _, _))
      .WillOnce(Invoke(DispatchEventOnCurrentTarget));
  EXPECT_TRUE(event_target->DispatchEvent(event, &exception_state));
}

}  // namespace
}  // namespace dom
}  // namespace cobalt
