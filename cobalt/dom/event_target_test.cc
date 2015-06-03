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

#include "cobalt/dom/testing/mock_event_listener.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::AllOf;
using ::testing::Eq;
using ::testing::InSequence;
using ::testing::InvokeWithoutArgs;
using ::testing::Pointee;
using ::testing::Property;
using ::testing::_;

namespace cobalt {
namespace dom {

using testing::MockEventListener;

namespace {

void ExpectHandleEventCall(const scoped_refptr<MockEventListener>& listener,
                           const scoped_refptr<Event>& event,
                           const scoped_refptr<EventTarget>& target) {
  // Note that we must pass the raw pointer to avoid reference counting issue.
  EXPECT_CALL(
      *listener,
      HandleEvent(AllOf(
          Eq(event.get()), Pointee(Property(&Event::target, Eq(target.get()))),
          Pointee(Property(&Event::current_target, Eq(target.get()))),
          Pointee(Property(&Event::event_phase, Eq(Event::kAtTarget))),
          Pointee(Property(&Event::IsBeingDispatched, Eq(true))))))
      .RetiresOnSaturation();
}

void ExpectHandleEventCall(const scoped_refptr<MockEventListener>& listener,
                           const scoped_refptr<Event>& event,
                           const scoped_refptr<EventTarget>& target,
                           void (Event::*function)(void)) {
  // Note that we must pass the raw pointer to avoid reference counting issue.
  EXPECT_CALL(
      *listener,
      HandleEvent(AllOf(
          Eq(event.get()), Pointee(Property(&Event::target, Eq(target.get()))),
          Pointee(Property(&Event::current_target, Eq(target.get()))),
          Pointee(Property(&Event::event_phase, Eq(Event::kAtTarget))),
          Pointee(Property(&Event::IsBeingDispatched, Eq(true))))))
      .WillOnce(InvokeWithoutArgs(event.get(), function))
      .RetiresOnSaturation();
}

void ExpectNoHandleEventCall(const scoped_refptr<MockEventListener>& listener) {
  EXPECT_CALL(*listener, HandleEvent(_)).Times(0);
}

}  // namespace

TEST(EventTargetTest, SingleEventListenerFired) {
  scoped_refptr<EventTarget> event_target = new EventTarget;
  scoped_refptr<Event> event = new Event("fired");
  scoped_refptr<MockEventListener> event_listener =
      MockEventListener::CreateAsNonAttribute();

  ExpectHandleEventCall(event_listener, event, event_target);
  event_target->AddEventListener("fired", event_listener, false);
  EXPECT_TRUE(event_target->DispatchEvent(event));
}

TEST(EventTargetTest, SingleEventListenerNotFired) {
  scoped_refptr<EventTarget> event_target = new EventTarget;
  scoped_refptr<Event> event = new Event("fired");
  scoped_refptr<MockEventListener> event_listener =
      MockEventListener::CreateAsNonAttribute();

  ExpectNoHandleEventCall(event_listener);
  event_target->AddEventListener("notfired", event_listener, false);
  EXPECT_TRUE(event_target->DispatchEvent(event));
}

// Test if multiple event listeners of different event types can be added and
// fired properly.
TEST(EventTargetTest, MultipleEventListeners) {
  scoped_refptr<EventTarget> event_target = new EventTarget;
  scoped_refptr<Event> event = new Event("fired");
  scoped_refptr<MockEventListener> event_listener_fired_1 =
      MockEventListener::CreateAsNonAttribute();
  scoped_refptr<MockEventListener> event_listener_fired_2 =
      MockEventListener::CreateAsNonAttribute();
  scoped_refptr<MockEventListener> event_listener_not_fired =
      MockEventListener::CreateAsNonAttribute();

  InSequence in_sequence;
  ExpectHandleEventCall(event_listener_fired_1, event, event_target);
  ExpectHandleEventCall(event_listener_fired_2, event, event_target);
  ExpectNoHandleEventCall(event_listener_not_fired);

  event_target->AddEventListener("fired", event_listener_fired_1, false);
  event_target->AddEventListener("notfired", event_listener_not_fired, false);
  event_target->AddEventListener("fired", event_listener_fired_2, true);

  EXPECT_TRUE(event_target->DispatchEvent(event));
}

// Test if event listener can be added and later removed.
TEST(EventTargetTest, AddRemoveEventListener) {
  scoped_refptr<EventTarget> event_target = new EventTarget;
  scoped_refptr<Event> event = new Event("fired");
  scoped_refptr<MockEventListener> event_listener =
      MockEventListener::CreateAsNonAttribute();

  ExpectHandleEventCall(event_listener, event, event_target);
  event_target->AddEventListener("fired", event_listener, false);
  EXPECT_TRUE(event_target->DispatchEvent(event));

  ExpectNoHandleEventCall(event_listener);
  event_target->RemoveEventListener("fired", event_listener, false);
  EXPECT_TRUE(event_target->DispatchEvent(event));

  ExpectHandleEventCall(event_listener, event, event_target);
  event_target->AddEventListener("fired", event_listener, false);
  EXPECT_TRUE(event_target->DispatchEvent(event));
}

// Test if attribute event listener works.
TEST(EventTargetTest, AttributeListener) {
  scoped_refptr<EventTarget> event_target = new EventTarget;
  scoped_refptr<Event> event = new Event("fired");
  scoped_refptr<MockEventListener> non_attribute_event_listener =
      MockEventListener::CreateAsNonAttribute();
  scoped_refptr<MockEventListener> attribute_event_listener_1 =
      MockEventListener::CreateAsAttribute();
  scoped_refptr<MockEventListener> attribute_event_listener_2 =
      MockEventListener::CreateAsAttribute();

  event_target->AddEventListener("fired", non_attribute_event_listener, false);

  ExpectHandleEventCall(non_attribute_event_listener, event, event_target);
  ExpectHandleEventCall(attribute_event_listener_1, event, event_target);
  event_target->SetAttributeEventListener("fired", attribute_event_listener_1);
  EXPECT_TRUE(event_target->DispatchEvent(event));

  ExpectHandleEventCall(non_attribute_event_listener, event, event_target);
  ExpectNoHandleEventCall(attribute_event_listener_1);
  ExpectHandleEventCall(attribute_event_listener_2, event, event_target);
  event_target->SetAttributeEventListener("fired", attribute_event_listener_2);
  EXPECT_TRUE(event_target->DispatchEvent(event));

  ExpectHandleEventCall(non_attribute_event_listener, event, event_target);
  ExpectNoHandleEventCall(attribute_event_listener_1);
  ExpectNoHandleEventCall(attribute_event_listener_2);
  event_target->SetAttributeEventListener("fired", NULL);
  EXPECT_TRUE(event_target->DispatchEvent(event));
}

// Test if one event listener can be used by multiple events.
TEST(EventTargetTest, EventListenerReuse) {
  scoped_refptr<EventTarget> event_target = new EventTarget;
  scoped_refptr<Event> event_1 = new Event("fired_1");
  scoped_refptr<Event> event_2 = new Event("fired_2");
  scoped_refptr<MockEventListener> event_listener =
      MockEventListener::CreateAsNonAttribute();

  ExpectHandleEventCall(event_listener, event_1, event_target);
  ExpectHandleEventCall(event_listener, event_2, event_target);
  event_target->AddEventListener("fired_1", event_listener, false);
  event_target->AddEventListener("fired_2", event_listener, false);
  EXPECT_TRUE(event_target->DispatchEvent(event_1));
  EXPECT_TRUE(event_target->DispatchEvent(event_2));

  ExpectHandleEventCall(event_listener, event_1, event_target);
  event_target->RemoveEventListener("fired_2", event_listener, false);
  EXPECT_TRUE(event_target->DispatchEvent(event_1));
  EXPECT_TRUE(event_target->DispatchEvent(event_2));

  ExpectHandleEventCall(event_listener, event_1, event_target);
  // The capture flag is not the same so the event will not be removed.
  event_target->RemoveEventListener("fired_1", event_listener, true);
  EXPECT_TRUE(event_target->DispatchEvent(event_1));
  EXPECT_TRUE(event_target->DispatchEvent(event_2));

  ExpectNoHandleEventCall(event_listener);
  event_target->RemoveEventListener("fired_1", event_listener, false);
  EXPECT_TRUE(event_target->DispatchEvent(event_1));
  EXPECT_TRUE(event_target->DispatchEvent(event_2));
}

TEST(EventTargetTest, StopPropagation) {
  scoped_refptr<EventTarget> event_target = new EventTarget;
  scoped_refptr<Event> event = new Event("fired");
  scoped_refptr<MockEventListener> event_listener_fired_1 =
      MockEventListener::CreateAsNonAttribute();
  scoped_refptr<MockEventListener> event_listener_fired_2 =
      MockEventListener::CreateAsNonAttribute();

  InSequence in_sequence;
  ExpectHandleEventCall(event_listener_fired_1, event, event_target,
                        &Event::StopPropagation);
  ExpectHandleEventCall(event_listener_fired_2, event, event_target);

  event_target->AddEventListener("fired", event_listener_fired_1, false);
  event_target->AddEventListener("fired", event_listener_fired_2, true);

  EXPECT_TRUE(event_target->DispatchEvent(event));
}

TEST(EventTargetTest, StopImmediatePropagation) {
  scoped_refptr<EventTarget> event_target = new EventTarget;
  scoped_refptr<Event> event = new Event("fired");
  scoped_refptr<MockEventListener> event_listener_fired_1 =
      MockEventListener::CreateAsNonAttribute();
  scoped_refptr<MockEventListener> event_listener_fired_2 =
      MockEventListener::CreateAsNonAttribute();

  ExpectHandleEventCall(event_listener_fired_1, event, event_target,
                        &Event::StopImmediatePropagation);
  ExpectNoHandleEventCall(event_listener_fired_2);

  event_target->AddEventListener("fired", event_listener_fired_1, false);
  event_target->AddEventListener("fired", event_listener_fired_2, true);

  EXPECT_TRUE(event_target->DispatchEvent(event));
}

TEST(EventTargetTest, PreventDefault) {
  scoped_refptr<Event> event;
  scoped_refptr<EventTarget> event_target = new EventTarget;
  scoped_refptr<MockEventListener> event_listener_fired =
      MockEventListener::CreateAsNonAttribute();

  event_target->AddEventListener("fired", event_listener_fired, false);
  event = new Event("fired", Event::kNotBubbles, Event::kNotCancelable);
  ExpectHandleEventCall(event_listener_fired, event, event_target,
                        &Event::PreventDefault);
  EXPECT_TRUE(event_target->DispatchEvent(event));

  event = new Event("fired", Event::kNotBubbles, Event::kCancelable);
  ExpectHandleEventCall(event_listener_fired, event, event_target,
                        &Event::PreventDefault);
  EXPECT_FALSE(event_target->DispatchEvent(event));
}

}  // namespace dom
}  // namespace cobalt
