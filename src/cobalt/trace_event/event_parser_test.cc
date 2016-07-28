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

#include "cobalt/trace_event/scoped_event_parser_trace.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::InSequence;
using ::testing::Invoke;
using ::testing::_;
using base::debug::TraceLog;
using cobalt::trace_event::EventParser;
using cobalt::trace_event::ScopedEventParserTrace;

// The tests in this file are all designed in a similar format that has three
// stages.
//
// 1) One or more Analyze() functions are defined which specify a function
//    for analyzing and testing the results of each finish ScopedEvent as they
//    arrive.
//
// 2) An expected sequence of flow end events is defined and they are setup
//    to each call the analysis function for further test verification of
//    the produced flow end event.
//
// 3) Finally a EventParserTest::Recorder object is constructed which starts
//    the recording of events, after which a number of TRACE commands are
//    issued that ultimately result in the pattern to be analyzed in stages 1
//    and 2.

namespace {

class MockEventParserEventHandler {
 public:
  MOCK_METHOD1(HandleFlowEndEvent,
               void(const scoped_refptr<EventParser::ScopedEvent>& event));
};

class EventParserTest : public ::testing::Test {
 public:
  virtual ~EventParserTest() {}

 protected:
  class Recorder {
   public:
    explicit Recorder(MockEventParserEventHandler* event_handler)
        : event_watcher_(
              base::Bind(&MockEventParserEventHandler::HandleFlowEndEvent,
                         base::Unretained(event_handler))) {}

   private:
    ScopedEventParserTrace event_watcher_;
  };

  MockEventParserEventHandler event_handler_;
};

// Useful for using base::Bind() along with GMock actions.
ACTION_P(InvokeCallback1, callback) { callback.Run(arg0); }

const char* kNullCharP = NULL;

// Helper function for finding the latest end time amongst the given event
// and all its children.  This is useful for testing that the given event's
// end flow time is equal to this value.
void ExpectLatestEndTimeOfDescendantsEqualToEndFlowTime(
    const scoped_refptr<EventParser::ScopedEvent>& event) {
  base::TimeTicks latest = event->end_event()->timestamp();
  for (size_t i = 0; i < event->children().size(); ++i) {
    ExpectLatestEndTimeOfDescendantsEqualToEndFlowTime(event->children()[i]);

    base::TimeTicks child_latest = *event->children()[i]->end_flow_time();
    if (child_latest - latest > base::TimeDelta()) {
      latest = child_latest;
    }
  }
  ASSERT_TRUE(static_cast<bool>(event->end_flow_time()));
  DCHECK_EQ(event->end_flow_time()->ToInternalValue(),
            latest.ToInternalValue());
}

// SimpleAnalyzeEvent performs a basic integrity check on the event passed in.
// In particular, it checks that the event's name matches the passed in
// event_name, that the event's parent's name matches the passed in parent_name
// or has no parent if parent_name is NULL, and finally it checks that the
// event has the specified number of children.
void SimpleAnalyzeEvent(const char* event_name, const char* parent_name,
                        int num_children,
                        const scoped_refptr<EventParser::ScopedEvent>& event) {
  // Ensure that the correct names have been stored in the begin event.
  EXPECT_STREQ(event_name, event->begin_event().name());

  // Ensure that the end event exists and has the correct names stored.
  EXPECT_TRUE(static_cast<bool>(event->end_event()));
  EXPECT_STREQ(event_name, event->end_event()->name());

  // The end event should always have a time tick larger than the begin
  // event.
  EXPECT_LE(base::TimeDelta(),
            event->end_event()->timestamp() - event->begin_event().timestamp());

  if (parent_name) {
    ASSERT_TRUE(event->parent());
    EXPECT_STREQ(parent_name, event->parent()->begin_event().name());

    // Test that we are one of our parents' children.
    const std::vector<scoped_refptr<EventParser::ScopedEvent> >& children =
        event->parent()->children();
    bool found = false;
    for (size_t i = 0; i < children.size(); ++i) {
      if (children[i] == event) {
        found = true;
        break;
      }
    }
    EXPECT_TRUE(found);
  }

  EXPECT_EQ(num_children, event->children().size());

  // Check that our end flow time is equal to the end time of one of our
  // descendants with the latest end time.
  ExpectLatestEndTimeOfDescendantsEqualToEndFlowTime(event);
}

}  // namespace

TEST_F(EventParserTest, SingleSimpleScopedEventTest) {
  {
    InSequence s;
    EXPECT_CALL(event_handler_, HandleFlowEndEvent(_)).WillOnce(InvokeCallback1(
        base::Bind(&SimpleAnalyzeEvent, "test_event", kNullCharP, 0)));
  }

  Recorder recorder(&event_handler_);
  TRACE_EVENT0("EventParserTest", "test_event");
}

TEST_F(EventParserTest, MultipleSiblingScopedEventTest) {
  {
    InSequence s;
    EXPECT_CALL(event_handler_, HandleFlowEndEvent(_)).WillOnce(InvokeCallback1(
        base::Bind(&SimpleAnalyzeEvent, "test_event_1", kNullCharP, 0)));
    EXPECT_CALL(event_handler_, HandleFlowEndEvent(_)).WillOnce(InvokeCallback1(
        base::Bind(&SimpleAnalyzeEvent, "test_event_2", kNullCharP, 0)));
    EXPECT_CALL(event_handler_, HandleFlowEndEvent(_)).WillOnce(InvokeCallback1(
        base::Bind(&SimpleAnalyzeEvent, "test_event_3", kNullCharP, 0)));
  }

  Recorder recorder(&event_handler_);
  { TRACE_EVENT0("EventParserTest", "test_event_1"); }
  { TRACE_EVENT0("EventParserTest", "test_event_2"); }
  { TRACE_EVENT0("EventParserTest", "test_event_3"); }
}

TEST_F(EventParserTest, SimpleNestedScopedEventsTest) {
  {
    InSequence s;
    EXPECT_CALL(event_handler_, HandleFlowEndEvent(_)).WillOnce(InvokeCallback1(
        base::Bind(&SimpleAnalyzeEvent, "test_event_3", "test_event_2", 0)));
    EXPECT_CALL(event_handler_, HandleFlowEndEvent(_)).WillOnce(InvokeCallback1(
        base::Bind(&SimpleAnalyzeEvent, "test_event_2", "test_event_1", 1)));
    EXPECT_CALL(event_handler_, HandleFlowEndEvent(_)).WillOnce(InvokeCallback1(
        base::Bind(&SimpleAnalyzeEvent, "test_event_1", kNullCharP, 1)));
  }

  Recorder recorder(&event_handler_);
  TRACE_EVENT0("EventParserTest", "test_event_1");
  TRACE_EVENT0("EventParserTest", "test_event_2");
  TRACE_EVENT0("EventParserTest", "test_event_3");
}

TEST_F(EventParserTest, MultipleNestedScopedEventsTest) {
  {
    InSequence s;
    EXPECT_CALL(event_handler_, HandleFlowEndEvent(_)).WillOnce(InvokeCallback1(
        base::Bind(&SimpleAnalyzeEvent, "test_event_2", "test_event_1", 0)));
    EXPECT_CALL(event_handler_, HandleFlowEndEvent(_)).WillOnce(InvokeCallback1(
        base::Bind(&SimpleAnalyzeEvent, "test_event_3", "test_event_1", 0)));
    EXPECT_CALL(event_handler_, HandleFlowEndEvent(_)).WillOnce(InvokeCallback1(
        base::Bind(&SimpleAnalyzeEvent, "test_event_1", kNullCharP, 2)));
  }

  Recorder recorder(&event_handler_);
  TRACE_EVENT0("EventParserTest", "test_event_1");
  { TRACE_EVENT0("EventParserTest", "test_event_2"); }
  { TRACE_EVENT0("EventParserTest", "test_event_3"); }
}

TEST_F(EventParserTest, SimpleFlowTest) {
  {
    InSequence s;
    EXPECT_CALL(event_handler_, HandleFlowEndEvent(_)).WillOnce(InvokeCallback1(
        base::Bind(&SimpleAnalyzeEvent, "test_flow", kNullCharP, 0)));
  }

  Recorder recorder(&event_handler_);
  TRACE_EVENT_FLOW_BEGIN0("EventParserTest", "test_flow", 1);
  TRACE_EVENT_FLOW_END0("EventParserTest", "test_flow", 1);
}

TEST_F(EventParserTest, ParallelFlowTest) {
  {
    InSequence s;
    EXPECT_CALL(event_handler_, HandleFlowEndEvent(_)).WillOnce(InvokeCallback1(
        base::Bind(&SimpleAnalyzeEvent, "test_flow_1", kNullCharP, 0)));
    EXPECT_CALL(event_handler_, HandleFlowEndEvent(_)).WillOnce(InvokeCallback1(
        base::Bind(&SimpleAnalyzeEvent, "test_flow_3", kNullCharP, 0)));
    EXPECT_CALL(event_handler_, HandleFlowEndEvent(_)).WillOnce(InvokeCallback1(
        base::Bind(&SimpleAnalyzeEvent, "test_flow_2", kNullCharP, 0)));
  }

  Recorder recorder(&event_handler_);
  TRACE_EVENT_FLOW_BEGIN0("EventParserTest", "test_flow_1", 1);
  TRACE_EVENT_FLOW_BEGIN0("EventParserTest", "test_flow_2", 2);
  TRACE_EVENT_FLOW_BEGIN0("EventParserTest", "test_flow_3", 3);

  TRACE_EVENT_FLOW_END0("EventParserTest", "test_flow_1", 1);
  TRACE_EVENT_FLOW_END0("EventParserTest", "test_flow_3", 3);
  TRACE_EVENT_FLOW_END0("EventParserTest", "test_flow_2", 2);
}

namespace {
void AnalyzeFlowSteps(const char* event_name, const char* step_1_name,
                      const char* step_2_name,
                      const scoped_refptr<EventParser::ScopedEvent>& event) {
  SimpleAnalyzeEvent(event_name, kNullCharP, 0, event);

  ASSERT_EQ(2, event->instant_events().size());
  EXPECT_STREQ(step_1_name,
               event->instant_events()[0].arg_values()[0].as_string);
  EXPECT_STREQ(step_2_name,
               event->instant_events()[1].arg_values()[0].as_string);
}
}  // namespace

TEST_F(EventParserTest, FlowHasStepTest) {
  {
    InSequence s;
    EXPECT_CALL(event_handler_, HandleFlowEndEvent(_)).WillOnce(
        InvokeCallback1(base::Bind(&AnalyzeFlowSteps, "test_flow",
                                   "test_flow_step_1", "test_flow_step_2")));
  }

  Recorder recorder(&event_handler_);
  TRACE_EVENT_FLOW_BEGIN0("EventParserTest", "test_flow", 1);
  TRACE_EVENT_FLOW_STEP0(
      "EventParserTest", "test_flow", 1, "test_flow_step_1");
  TRACE_EVENT_FLOW_STEP0(
      "EventParserTest", "test_flow", 1, "test_flow_step_2");
  TRACE_EVENT_FLOW_END0("EventParserTest", "test_flow", 1);
}

// Flows will be children of normal scoped events if they are *begun* within
// that scope.  In this case, all future TRACE_EVENT_FLOW_STEP and
// TRACE_EVENT_FLOW_END calls will refer to the same flow event with the
// original parent from the call to TRACE_EVENT_FLOW_BEGIN.  Note that flows
// can extend a scoped event's flow end time beyond the scopes end time.
TEST_F(EventParserTest, FlowIsChildOfEventTest) {
  {
    InSequence s;
    EXPECT_CALL(event_handler_, HandleFlowEndEvent(_)).WillOnce(InvokeCallback1(
        base::Bind(&SimpleAnalyzeEvent, "test_event_2", kNullCharP, 0)));
    EXPECT_CALL(event_handler_, HandleFlowEndEvent(_)).WillOnce(InvokeCallback1(
        base::Bind(&SimpleAnalyzeEvent, "test_flow", "test_event_1", 0)));
    EXPECT_CALL(event_handler_, HandleFlowEndEvent(_)).WillOnce(InvokeCallback1(
        base::Bind(&SimpleAnalyzeEvent, "test_event_1", kNullCharP, 1)));
    EXPECT_CALL(event_handler_, HandleFlowEndEvent(_)).WillOnce(InvokeCallback1(
        base::Bind(&SimpleAnalyzeEvent, "test_event_3", kNullCharP, 0)));
  }

  Recorder recorder(&event_handler_);
  {
    TRACE_EVENT0("EventParserTest", "test_event_1");
    TRACE_EVENT_FLOW_BEGIN0("EventParserTest", "test_flow", 1);
  }

  {
    TRACE_EVENT0("EventParserTest", "test_event_2");
    TRACE_EVENT_FLOW_STEP0("EventParserTest", "test_flow", 1,
                           "test_flow_step_1");
  }

  {
    TRACE_EVENT0("EventParserTest", "test_event_3");
    TRACE_EVENT_FLOW_END0("EventParserTest", "test_flow", 1);
  }
}

// It is possible to link an event as a child of a flow step by declaring
// the event immediately after the call to TRACE_EVENT_FLOW_STEP, and assigning
// the event the same name as the flow step.
TEST_F(EventParserTest, EventIsChildOfFlowStepTest) {
  {
    InSequence s;
    EXPECT_CALL(event_handler_, HandleFlowEndEvent(_)).WillOnce(InvokeCallback1(
        base::Bind(&SimpleAnalyzeEvent, "test_flow_step_1", "test_flow", 0)));
    EXPECT_CALL(event_handler_, HandleFlowEndEvent(_)).WillOnce(InvokeCallback1(
        base::Bind(&SimpleAnalyzeEvent, "test_flow", kNullCharP, 1)));
  }

  Recorder recorder(&event_handler_);
  TRACE_EVENT_FLOW_BEGIN0("EventParserTest", "test_flow", 1);

  {
    TRACE_EVENT_FLOW_STEP0("EventParserTest", "test_flow", 1,
                           "test_flow_step_1");
    TRACE_EVENT0("EventParserTest", "test_flow_step_1");
  }

  TRACE_EVENT_FLOW_END0("EventParserTest", "test_flow", 1);
}

// As a special case to support Chromium's base::MessageLoop tracing
// integration, it is possible to link an event as a child of a flow by
// declaring an event with the name "MessageLoop::RunTask" immediately after
// ending a flow with the name "MessageLoop::PostTask".
TEST_F(EventParserTest, EventIsChildOfFlowEndPostTaskTest) {
  {
    InSequence s;
    EXPECT_CALL(event_handler_, HandleFlowEndEvent(_)).WillOnce(
        InvokeCallback1(base::Bind(&SimpleAnalyzeEvent, "MessageLoop::RunTask",
                                   "MessageLoop::PostTask", 0)));
    EXPECT_CALL(event_handler_, HandleFlowEndEvent(_))
        .WillOnce(InvokeCallback1(base::Bind(
            &SimpleAnalyzeEvent, "MessageLoop::PostTask", kNullCharP, 1)));
  }

  Recorder recorder(&event_handler_);
  TRACE_EVENT_FLOW_BEGIN0("EventParserTest", "MessageLoop::PostTask", 1);
  TRACE_EVENT_FLOW_END0("EventParserTest", "MessageLoop::PostTask", 1);
  TRACE_EVENT0("EventParserTest", "MessageLoop::RunTask");
}

namespace {
void NopEventHandler(const scoped_refptr<EventParser::ScopedEvent>& event) {
  UNREFERENCED_PARAMETER(event);
}
}  // namespace

TEST_F(EventParserTest, VeryLongFlowChainsDoNotStackOverflow) {
  // This test ensures that when we construct very long chains of flow events,
  // the logic for processing the results does not result in a stack overflow
  // due to recursive calls.  Since GMock's "InSequence" also constructs
  // a tree model, it would crash in this test due to stack overflow, so it is
  // not used.
  static const int kFlowLength = 2000;

  ScopedEventParserTrace event_watcher(base::Bind(&NopEventHandler));
  for (int i = 0; i < kFlowLength; ++i) {
    if (i != 0) {
      TRACE_EVENT_FLOW_END0("EventParserTest", "MessageLoop::PostTask", i - 1);
    }
    TRACE_EVENT0("EventParserTest", "MessageLoop::RunTask");
    TRACE_EVENT_FLOW_BEGIN0("EventParserTest", "MessageLoop::PostTask", i);
  }
  TRACE_EVENT_FLOW_END0("EventParserTest", "MessageLoop::PostTask",
                        kFlowLength - 1);
  TRACE_EVENT0("EventParserTest", "MessageLoop::RunTask");
}

TEST_F(EventParserTest, VeryLongFlowChainsDoNotStackOverflowOnAbort) {
  // This test is similar to VeryLongFlowChainsDoNotStackOverflow except it
  // deals with events that are aborted versus events that are ended naturally.
  static const int kFlowLength = 2000;

  base::optional<ScopedEventParserTrace> event_watcher(
      base::in_place, base::Bind(&NopEventHandler));
  for (int i = 0; i < kFlowLength; ++i) {
    if (i != 0) {
      TRACE_EVENT_FLOW_END0("EventParserTest", "MessageLoop::PostTask", i - 1);
    }
    TRACE_EVENT0("EventParserTest", "MessageLoop::RunTask");
    TRACE_EVENT_FLOW_BEGIN0("EventParserTest", "MessageLoop::PostTask", i);
  }
  TRACE_EVENT_FLOW_END0("EventParserTest", "MessageLoop::PostTask",
                        kFlowLength - 1);
  TRACE_EVENT0("EventParserTest", "MessageLoop::RunTask");

  // Destroy the event watcher before the last TRACE_EVENT0() scope closes,
  // triggering an abort for the unfinished event flows.
  event_watcher = base::nullopt;
}
