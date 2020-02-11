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

#include "cobalt/dom/event.h"

#include "cobalt/dom/global_stats.h"
#include "base/time/time.h"
#include "cobalt/dom/testing/gtest_workarounds.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cobalt {
namespace dom {

class EventTest : public ::testing::Test {
 protected:
  EventTest(){
    EXPECT_TRUE(GlobalStats::GetInstance()->CheckNoLeaks());
  }
  ~EventTest() override {
    EXPECT_TRUE(GlobalStats::GetInstance()->CheckNoLeaks());
  }
};

TEST_F(EventTest, DefaultConstructor) {
  scoped_refptr<Event> event = new Event(base::Token("event"));

  EXPECT_EQ("event", event->type());
  EXPECT_EQ(NULL, event->target().get());
  EXPECT_EQ(NULL, event->current_target().get());
  EXPECT_EQ(Event::kNone, event->event_phase());
  EXPECT_FALSE(event->bubbles());
  EXPECT_FALSE(event->cancelable());
  EXPECT_FALSE(event->default_prevented());
  EXPECT_FALSE(event->IsBeingDispatched());
  EXPECT_FALSE(event->propagation_stopped());
  EXPECT_FALSE(event->immediate_propagation_stopped());
}

TEST_F(EventTest, NonDefaultConstructor) {
  scoped_refptr<Event> event =
      new Event(base::Token("event"), Event::kBubbles, Event::kCancelable);

  EXPECT_EQ("event", event->type());
  EXPECT_EQ(NULL, event->target().get());
  EXPECT_EQ(NULL, event->current_target().get());
  EXPECT_EQ(Event::kNone, event->event_phase());
  EXPECT_TRUE(event->bubbles());
  EXPECT_TRUE(event->cancelable());
  EXPECT_FALSE(event->default_prevented());
  EXPECT_FALSE(event->IsBeingDispatched());
  EXPECT_FALSE(event->propagation_stopped());
  EXPECT_FALSE(event->immediate_propagation_stopped());
}

TEST_F(EventTest, TimeStamp) {
  // Ensure that the creation timestamp of the event is in a reasonable range.
  // Since base::Time::Now() doesn't guarantee that the result is monotonically
  // increasing, we use a 60 seconds episilon as it is large enough to ensure
  // the test is not flaky but small enough to no being affected by any zoning
  // issue.
  uint64 now_in_js = static_cast<uint64>(base::Time::Now().ToJsTime());
  uint64 episilon_in_ms = base::Time::kMillisecondsPerSecond * 60;
  scoped_refptr<Event> event = new Event(base::Token("event"));

  EXPECT_GE(event->time_stamp(), now_in_js - episilon_in_ms);
}

TEST_F(EventTest, InitEvent) {
  scoped_refptr<Event> event = new Event(base::Token("event_1"));
  double time_stamp = static_cast<double>(event->time_stamp());

  event->StopImmediatePropagation();
  event->PreventDefault();

  event->InitEvent("event_2", true, true);

  EXPECT_EQ("event_2", event->type());
  EXPECT_TRUE(event->bubbles());
  EXPECT_TRUE(event->cancelable());
  EXPECT_EQ(time_stamp, event->time_stamp());
  EXPECT_FALSE(event->default_prevented());
  EXPECT_FALSE(event->propagation_stopped());
  EXPECT_FALSE(event->immediate_propagation_stopped());

  event->InitEvent("event_2", true, false);
  EXPECT_TRUE(event->bubbles());
  EXPECT_FALSE(event->cancelable());

  event->InitEvent("event_2", false, true);
  EXPECT_FALSE(event->bubbles());
  EXPECT_TRUE(event->cancelable());

  event->InitEvent("event_2", false, false);
  EXPECT_FALSE(event->bubbles());
  EXPECT_FALSE(event->cancelable());
}

TEST_F(EventTest, StopPropagation) {
  scoped_refptr<Event> event = new Event(base::Token("event"));
  event->StopPropagation();
  EXPECT_TRUE(event->propagation_stopped());
}

TEST_F(EventTest, StopImmediatePropagation) {
  scoped_refptr<Event> event = new Event(base::Token("event"));
  event->StopImmediatePropagation();
  EXPECT_TRUE(event->propagation_stopped());
  EXPECT_TRUE(event->immediate_propagation_stopped());
}

TEST_F(EventTest, PreventDefault) {
  scoped_refptr<Event> event = new Event(
      base::Token("event"), Event::kNotBubbles, Event::kNotCancelable);
  event->PreventDefault();
  EXPECT_FALSE(event->default_prevented());
  // explicitly init it to non-cancelable.
  event =
      new Event(base::Token("event"), Event::kNotBubbles, Event::kCancelable);
  event->PreventDefault();
  EXPECT_TRUE(event->default_prevented());
}

TEST_F(EventTest, EventPhase) {
  scoped_refptr<Event> event = new Event(base::Token("event"));
  event->set_event_phase(Event::kCapturingPhase);
  EXPECT_EQ(Event::kCapturingPhase, event->event_phase());
  EXPECT_TRUE(event->IsBeingDispatched());
  event->set_event_phase(Event::kAtTarget);
  EXPECT_EQ(Event::kAtTarget, event->event_phase());
  EXPECT_TRUE(event->IsBeingDispatched());
  event->set_event_phase(Event::kBubblingPhase);
  EXPECT_EQ(Event::kBubblingPhase, event->event_phase());
  EXPECT_TRUE(event->IsBeingDispatched());
  event->set_event_phase(Event::kNone);
  EXPECT_EQ(Event::kNone, event->event_phase());
  EXPECT_FALSE(event->IsBeingDispatched());
}

}  // namespace dom
}  // namespace cobalt
