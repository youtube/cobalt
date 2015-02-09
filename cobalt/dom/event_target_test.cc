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
#include "cobalt/script/script_object_handle_visitor.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;

// Use NiceMock as we don't care about EqualTo or MarkJSObjectAsNotCollectable
// calls on the listener in most cases.
typedef ::testing::NiceMock<cobalt::dom::testing::MockEventListener>
    NiceMockEventListener;

namespace {

class TestScriptObjectHandleVisitor
    : public cobalt::script::ScriptObjectHandleVisitor {
 private:
  void Visit(cobalt::script::ScriptObjectHandle* handle) OVERRIDE{};
};

}  // namespace

namespace cobalt {
namespace dom {

TEST(EventTargetTest, SingleEventListenerFiredTest) {
  scoped_refptr<EventTarget> event_target = new EventTarget;
  scoped_refptr<Event> event = new Event("fired");
  scoped_refptr<NiceMockEventListener> event_listener =
      new NiceMockEventListener;

  EXPECT_CALL(*event_listener, HandleEvent(event));
  event_target->AddEventListener("fired", event_listener, false);
  event_target->DispatchEvent(event);
}

TEST(EventTargetTest, SingleEventListenerNotFiredTest) {
  scoped_refptr<EventTarget> event_target = new EventTarget;
  scoped_refptr<Event> event = new Event("fired");
  scoped_refptr<NiceMockEventListener> event_listener =
      new NiceMockEventListener;

  EXPECT_CALL(*event_listener, HandleEvent(_)).Times(0);
  event_target->AddEventListener("notfired", event_listener, false);
  event_target->DispatchEvent(event);
}

// Test if multiple event listeners of different event types can be added and
// fired properly.
TEST(EventTargetTest, MultipleEventListenersTest) {
  scoped_refptr<EventTarget> event_target = new EventTarget;
  scoped_refptr<Event> event = new Event("fired");
  scoped_refptr<NiceMockEventListener> event_listener_fired_1 =
      new NiceMockEventListener;
  scoped_refptr<NiceMockEventListener> event_listener_fired_2 =
      new NiceMockEventListener;
  scoped_refptr<NiceMockEventListener> event_listener_not_fired =
      new NiceMockEventListener;

  EXPECT_CALL(*event_listener_fired_1, HandleEvent(event));
  EXPECT_CALL(*event_listener_fired_2, HandleEvent(event));
  EXPECT_CALL(*event_listener_not_fired, HandleEvent(event)).Times(0);

  event_target->AddEventListener("fired", event_listener_fired_1, false);
  event_target->AddEventListener("notfired", event_listener_not_fired, false);
  event_target->AddEventListener("fired", event_listener_fired_2, true);

  event_target->DispatchEvent(event);
}

// Test if event listener can be added and later removed.
TEST(EventTargetTest, AddRemoveEventListenerTest) {
  scoped_refptr<EventTarget> event_target = new EventTarget;
  scoped_refptr<Event> event = new Event("fired");
  scoped_refptr<NiceMockEventListener> event_listener =
      new NiceMockEventListener;

  EXPECT_CALL(*event_listener, HandleEvent(_));
  event_target->AddEventListener("fired", event_listener, false);
  event_target->DispatchEvent(event);

  EXPECT_CALL(*event_listener, HandleEvent(_)).Times(0);
  event_target->RemoveEventListener("fired", event_listener, false);
  event_target->DispatchEvent(event);

  EXPECT_CALL(*event_listener, HandleEvent(_));
  event_target->AddEventListener("fired", event_listener, false);
  event_target->DispatchEvent(event);
}

// Test if one event listener can be used by multiple events.
TEST(EventTargetTest, EventListenerReuseTest) {
  scoped_refptr<EventTarget> event_target = new EventTarget;
  scoped_refptr<Event> event_1 = new Event("fired_1");
  scoped_refptr<Event> event_2 = new Event("fired_2");
  scoped_refptr<NiceMockEventListener> event_listener =
      new NiceMockEventListener;

  EXPECT_CALL(*event_listener, HandleEvent(event_1)).Times(1);
  EXPECT_CALL(*event_listener, HandleEvent(event_2)).Times(1);
  event_target->AddEventListener("fired_1", event_listener, false);
  event_target->AddEventListener("fired_2", event_listener, false);
  event_target->DispatchEvent(event_1);
  event_target->DispatchEvent(event_2);

  EXPECT_CALL(*event_listener, HandleEvent(event_1)).Times(1);
  event_target->RemoveEventListener("fired_2", event_listener, false);
  event_target->DispatchEvent(event_1);
  event_target->DispatchEvent(event_2);

  EXPECT_CALL(*event_listener, HandleEvent(event_1)).Times(1);
  // The capture flag is not the same so the event will not be removed.
  event_target->RemoveEventListener("fired_1", event_listener, true);
  event_target->DispatchEvent(event_1);
  event_target->DispatchEvent(event_2);

  EXPECT_CALL(*event_listener, HandleEvent(_)).Times(0);
  event_target->RemoveEventListener("fired_1", event_listener, false);
  event_target->DispatchEvent(event_1);
  event_target->DispatchEvent(event_2);
}

// Test MarkJSObjectAsNotCollectable on added event listener is called when
// it is called on the EventTarget.
TEST(EventTargetTest, MarkJSObjectAsNotCollectableTest) {
  scoped_refptr<EventTarget> event_target = new EventTarget;
  scoped_refptr<NiceMockEventListener> event_listener =
      new NiceMockEventListener;
  TestScriptObjectHandleVisitor script_object_handle_visitor;

  EXPECT_CALL(*event_listener,
              MarkJSObjectAsNotCollectable(&script_object_handle_visitor))
      .Times(::testing::AtLeast(1));
  event_target->AddEventListener("fired", event_listener, false);
  event_target->MarkJSObjectAsNotCollectable(&script_object_handle_visitor);

  EXPECT_CALL(*event_listener, MarkJSObjectAsNotCollectable(
                                   &script_object_handle_visitor)).Times(0);
  event_target->RemoveEventListener("fired", event_listener, false);
  event_target->MarkJSObjectAsNotCollectable(&script_object_handle_visitor);
}

}  // namespace dom
}  // namespace cobalt
