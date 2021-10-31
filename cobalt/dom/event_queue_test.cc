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

#include "cobalt/dom/event_queue.h"

#include <memory>

#include "base/message_loop/message_loop.h"
#include "cobalt/dom/event.h"
#include "cobalt/dom/event_target.h"
#include "cobalt/dom/testing/mock_event_listener.h"
#include "cobalt/dom/testing/stub_environment_settings.h"
#include "cobalt/script/testing/fake_script_value.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::AllOf;
using ::testing::Eq;
using ::testing::Property;

namespace cobalt {
namespace dom {

using ::cobalt::script::testing::FakeScriptValue;
using testing::MockEventListener;

class EventQueueTest : public ::testing::Test {
 protected:
  void ExpectHandleEventCallWithEventAndTarget(
      const MockEventListener* listener, const scoped_refptr<Event>& event,
      const scoped_refptr<EventTarget>& target) {
    // Note that we must pass the raw pointer to avoid reference counting issue.
    EXPECT_CALL(
        *listener,
        HandleEvent(_,
                    AllOf(Eq(event.get()),
                          Pointee(Property(&Event::target, Eq(target.get())))),
                    _))
        .RetiresOnSaturation();
  }
  testing::StubEnvironmentSettings environment_settings_;
  base::MessageLoop message_loop_;
};

TEST_F(EventQueueTest, EventWithoutTargetTest) {
  scoped_refptr<EventTarget> event_target =
      new EventTarget(&environment_settings_);
  scoped_refptr<Event> event = new Event(base::Token("event"));
  std::unique_ptr<MockEventListener> event_listener =
      MockEventListener::Create();
  EventQueue event_queue(event_target.get());

  event_target->AddEventListener(
      "event", FakeScriptValue<EventListener>(event_listener.get()), false);
  ExpectHandleEventCallWithEventAndTarget(event_listener.get(), event,
                                          event_target);

  event_queue.Enqueue(event);
  base::RunLoop().RunUntilIdle();
}

TEST_F(EventQueueTest, EventWithTargetTest) {
  scoped_refptr<EventTarget> event_target =
      new EventTarget(&environment_settings_);
  scoped_refptr<Event> event = new Event(base::Token("event"));
  std::unique_ptr<MockEventListener> event_listener =
      MockEventListener::Create();
  EventQueue event_queue(event_target.get());

  event->set_target(event_target);
  event_target->AddEventListener(
      "event", FakeScriptValue<EventListener>(event_listener.get()), false);
  ExpectHandleEventCallWithEventAndTarget(event_listener.get(), event,
                                          event_target);

  event_queue.Enqueue(event);
  base::RunLoop().RunUntilIdle();
}

TEST_F(EventQueueTest, CancelAllEventsTest) {
  scoped_refptr<EventTarget> event_target =
      new EventTarget(&environment_settings_);
  scoped_refptr<Event> event = new Event(base::Token("event"));
  std::unique_ptr<MockEventListener> event_listener =
      MockEventListener::Create();
  EventQueue event_queue(event_target.get());

  event->set_target(event_target);
  event_target->AddEventListener(
      "event", FakeScriptValue<EventListener>(event_listener.get()), false);
  event_listener->ExpectNoHandleEventCall();

  event_queue.Enqueue(event);
  event_queue.CancelAllEvents();
  base::RunLoop().RunUntilIdle();
}

// We only test if the EventQueue doesn't mess up the target we set. The
// correctness of event propagation like capturing or bubbling are tested in
// the unit tests of EventTarget.
TEST_F(EventQueueTest, EventWithDifferentTargetTest) {
  scoped_refptr<EventTarget> event_target_1 =
      new EventTarget(&environment_settings_);
  scoped_refptr<EventTarget> event_target_2 =
      new EventTarget(&environment_settings_);
  scoped_refptr<Event> event = new Event(base::Token("event"));
  std::unique_ptr<MockEventListener> event_listener =
      MockEventListener::Create();

  EventQueue event_queue(event_target_1.get());

  event->set_target(event_target_2);
  event_target_2->AddEventListener(
      "event", FakeScriptValue<EventListener>(event_listener.get()), false);
  ExpectHandleEventCallWithEventAndTarget(event_listener.get(), event,
                                          event_target_2);

  event_queue.Enqueue(event);
  base::RunLoop().RunUntilIdle();
}

}  // namespace dom
}  // namespace cobalt
