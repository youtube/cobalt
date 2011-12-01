// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/test/trace_event_analyzer.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace trace_analyzer {

namespace {

class TraceEventAnalyzerTest : public testing::Test {
 public:
  void ManualSetUp();
  void OnTraceDataCollected(
      scoped_refptr<base::debug::TraceLog::RefCountedString> json_events_str);
  void BeginTracing();
  void EndTracing();

  base::debug::TraceResultBuffer::SimpleOutput output_;
  base::debug::TraceResultBuffer buffer_;
};

void TraceEventAnalyzerTest::ManualSetUp() {
  base::debug::TraceLog::Resurrect();
  base::debug::TraceLog* tracelog = base::debug::TraceLog::GetInstance();
  ASSERT_TRUE(tracelog);
  tracelog->SetOutputCallback(
    base::Bind(&TraceEventAnalyzerTest::OnTraceDataCollected,
               base::Unretained(this)));
  buffer_.SetOutputCallback(output_.GetCallback());
  output_.json_output.clear();
}

void TraceEventAnalyzerTest::OnTraceDataCollected(
    scoped_refptr<base::debug::TraceLog::RefCountedString> json_events_str) {
  buffer_.AddFragment(json_events_str->data);
}

void TraceEventAnalyzerTest::BeginTracing() {
  output_.json_output.clear();
  buffer_.Start();
  base::debug::TraceLog::GetInstance()->SetEnabled(true);
}

void TraceEventAnalyzerTest::EndTracing() {
  base::debug::TraceLog::GetInstance()->SetEnabled(false);
  buffer_.Finish();
}

}  // namespace

TEST_F(TraceEventAnalyzerTest, NoEvents) {
  using namespace trace_analyzer;
  ManualSetUp();

  // Create an empty JSON event string:
  buffer_.Start();
  buffer_.Finish();

  scoped_ptr<TraceAnalyzer>
      analyzer(TraceAnalyzer::Create(output_.json_output));
  ASSERT_TRUE(analyzer.get());

  // Search for all events and verify that nothing is returned.
  TraceAnalyzer::TraceEventVector found;
  analyzer->FindEvents(Query::Bool(true), &found);
  EXPECT_EQ(0u, found.size());
}

TEST_F(TraceEventAnalyzerTest, TraceEvent) {
  using namespace trace_analyzer;
  ManualSetUp();

  int int_num = 2;
  double double_num = 3.5;
  const char* str = "the string";

  TraceEvent event;
  event.arg_numbers["false"] = 0.0;
  event.arg_numbers["true"] = 1.0;
  event.arg_numbers["int"] = static_cast<double>(int_num);
  event.arg_numbers["double"] = double_num;
  event.arg_strings["string"] = str;

  ASSERT_TRUE(event.HasNumberArg("false"));
  ASSERT_TRUE(event.HasNumberArg("true"));
  ASSERT_TRUE(event.HasNumberArg("int"));
  ASSERT_TRUE(event.HasNumberArg("double"));
  ASSERT_TRUE(event.HasStringArg("string"));
  ASSERT_FALSE(event.HasNumberArg("notfound"));
  ASSERT_FALSE(event.HasStringArg("notfound"));

  EXPECT_FALSE(event.GetKnownArgAsBool("false"));
  EXPECT_TRUE(event.GetKnownArgAsBool("true"));
  EXPECT_EQ(int_num, event.GetKnownArgAsInt("int"));
  EXPECT_EQ(double_num, event.GetKnownArgAsDouble("double"));
  EXPECT_STREQ(str, event.GetKnownArgAsString("string").c_str());
}

TEST_F(TraceEventAnalyzerTest, QueryEventMember) {
  using namespace trace_analyzer;
  ManualSetUp();

  TraceEvent event;
  event.thread.process_id = 3;
  event.thread.thread_id = 4;
  event.timestamp = 1.5;
  event.phase = base::debug::TRACE_EVENT_PHASE_BEGIN;
  event.category = "category";
  event.name = "name";
  event.arg_numbers["num"] = 7.0;
  event.arg_strings["str"] = "the string";

  // Other event with all different members:
  TraceEvent other;
  other.thread.process_id = 5;
  other.thread.thread_id = 6;
  other.timestamp = 2.5;
  other.phase = base::debug::TRACE_EVENT_PHASE_END;
  other.category = "category2";
  other.name = "name2";
  other.arg_numbers["num2"] = 8.0;
  other.arg_strings["str2"] = "the string 2";

  event.other_event = &other;
  ASSERT_TRUE(event.has_other_event());
  double duration = event.GetAbsTimeToOtherEvent();

  Query event_pid = (Query(EVENT_PID) == Query::Int(event.thread.process_id));
  Query event_tid = (Query(EVENT_TID) == Query::Int(event.thread.thread_id));
  Query event_time = (Query(EVENT_TIME) == Query::Double(event.timestamp));
  Query event_duration = (Query(EVENT_DURATION) == Query::Double(duration));
  Query event_phase = (Query(EVENT_PHASE) == Query::Phase(event.phase));
  Query event_category =
      (Query(EVENT_CATEGORY) == Query::String(event.category));
  Query event_name = (Query(EVENT_NAME) == Query::String(event.name));
  Query event_has_arg1 = Query(EVENT_HAS_NUMBER_ARG, "num");
  Query event_has_arg2 = Query(EVENT_HAS_STRING_ARG, "str");
  Query event_arg1 =
      (Query(EVENT_ARG, "num") == Query::Double(event.arg_numbers["num"]));
  Query event_arg2 =
      (Query(EVENT_ARG, "str") == Query::String(event.arg_strings["str"]));
  Query event_has_other = Query(EVENT_HAS_OTHER);
  Query other_pid = (Query(OTHER_PID) == Query::Int(other.thread.process_id));
  Query other_tid = (Query(OTHER_TID) == Query::Int(other.thread.thread_id));
  Query other_time = (Query(OTHER_TIME) == Query::Double(other.timestamp));
  Query other_phase = (Query(OTHER_PHASE) == Query::Phase(other.phase));
  Query other_category =
      (Query(OTHER_CATEGORY) == Query::String(other.category));
  Query other_name = (Query(OTHER_NAME) == Query::String(other.name));
  Query other_has_arg1 = Query(OTHER_HAS_NUMBER_ARG, "num2");
  Query other_has_arg2 = Query(OTHER_HAS_STRING_ARG, "str2");
  Query other_arg1 =
      (Query(OTHER_ARG, "num2") == Query::Double(other.arg_numbers["num2"]));
  Query other_arg2 =
      (Query(OTHER_ARG, "str2") == Query::String(other.arg_strings["str2"]));

  EXPECT_TRUE(event_pid.Evaluate(event));
  EXPECT_TRUE(event_tid.Evaluate(event));
  EXPECT_TRUE(event_time.Evaluate(event));
  EXPECT_TRUE(event_duration.Evaluate(event));
  EXPECT_TRUE(event_phase.Evaluate(event));
  EXPECT_TRUE(event_category.Evaluate(event));
  EXPECT_TRUE(event_name.Evaluate(event));
  EXPECT_TRUE(event_has_arg1.Evaluate(event));
  EXPECT_TRUE(event_has_arg2.Evaluate(event));
  EXPECT_TRUE(event_arg1.Evaluate(event));
  EXPECT_TRUE(event_arg2.Evaluate(event));
  EXPECT_TRUE(event_has_other.Evaluate(event));
  EXPECT_TRUE(other_pid.Evaluate(event));
  EXPECT_TRUE(other_tid.Evaluate(event));
  EXPECT_TRUE(other_time.Evaluate(event));
  EXPECT_TRUE(other_phase.Evaluate(event));
  EXPECT_TRUE(other_category.Evaluate(event));
  EXPECT_TRUE(other_name.Evaluate(event));
  EXPECT_TRUE(other_has_arg1.Evaluate(event));
  EXPECT_TRUE(other_has_arg2.Evaluate(event));
  EXPECT_TRUE(other_arg1.Evaluate(event));
  EXPECT_TRUE(other_arg2.Evaluate(event));

  // Evaluate event queries against other to verify the queries fail when the
  // event members are wrong.
  EXPECT_FALSE(event_pid.Evaluate(other));
  EXPECT_FALSE(event_tid.Evaluate(other));
  EXPECT_FALSE(event_time.Evaluate(other));
  EXPECT_FALSE(event_duration.Evaluate(other));
  EXPECT_FALSE(event_phase.Evaluate(other));
  EXPECT_FALSE(event_category.Evaluate(other));
  EXPECT_FALSE(event_name.Evaluate(other));
  EXPECT_FALSE(event_has_arg1.Evaluate(other));
  EXPECT_FALSE(event_has_arg2.Evaluate(other));
  EXPECT_FALSE(event_arg1.Evaluate(other));
  EXPECT_FALSE(event_arg2.Evaluate(other));
  EXPECT_FALSE(event_has_other.Evaluate(other));
}

TEST_F(TraceEventAnalyzerTest, BooleanOperators) {
  using namespace trace_analyzer;
  ManualSetUp();

  BeginTracing();
  {
    TRACE_EVENT_INSTANT1("cat1", "name1", "num", 1);
    TRACE_EVENT_INSTANT1("cat1", "name2", "num", 2);
    TRACE_EVENT_INSTANT1("cat2", "name3", "num", 3);
    TRACE_EVENT_INSTANT1("cat2", "name4", "num", 4);
  }
  EndTracing();

  scoped_ptr<TraceAnalyzer>
      analyzer(TraceAnalyzer::Create(output_.json_output));
  ASSERT_TRUE(!!analyzer.get());

  TraceAnalyzer::TraceEventVector found;

  // ==

  analyzer->FindEvents(Query(EVENT_CATEGORY) == Query::String("cat1"), &found);
  ASSERT_EQ(2u, found.size());
  EXPECT_STREQ("name1", found[0]->name.c_str());
  EXPECT_STREQ("name2", found[1]->name.c_str());

  analyzer->FindEvents(Query(EVENT_ARG, "num") == Query::Int(2), &found);
  ASSERT_EQ(1u, found.size());
  EXPECT_STREQ("name2", found[0]->name.c_str());

  // !=

  analyzer->FindEvents(Query(EVENT_CATEGORY) != Query::String("cat1"), &found);
  ASSERT_EQ(2u, found.size());
  EXPECT_STREQ("name3", found[0]->name.c_str());
  EXPECT_STREQ("name4", found[1]->name.c_str());

  analyzer->FindEvents(Query(EVENT_ARG, "num") != Query::Int(2), &found);
  ASSERT_EQ(3u, found.size());
  EXPECT_STREQ("name1", found[0]->name.c_str());
  EXPECT_STREQ("name3", found[1]->name.c_str());
  EXPECT_STREQ("name4", found[2]->name.c_str());

  // <
  analyzer->FindEvents(Query(EVENT_ARG, "num") < Query::Int(2), &found);
  ASSERT_EQ(1u, found.size());
  EXPECT_STREQ("name1", found[0]->name.c_str());

  // <=
  analyzer->FindEvents(Query(EVENT_ARG, "num") <= Query::Int(2), &found);
  ASSERT_EQ(2u, found.size());
  EXPECT_STREQ("name1", found[0]->name.c_str());
  EXPECT_STREQ("name2", found[1]->name.c_str());

  // >
  analyzer->FindEvents(Query(EVENT_ARG, "num") > Query::Int(3), &found);
  ASSERT_EQ(1u, found.size());
  EXPECT_STREQ("name4", found[0]->name.c_str());

  // >=
  analyzer->FindEvents(Query(EVENT_ARG, "num") >= Query::Int(4), &found);
  ASSERT_EQ(1u, found.size());
  EXPECT_STREQ("name4", found[0]->name.c_str());

  // &&
  analyzer->FindEvents(Query(EVENT_NAME) != Query::String("name1") &&
                       Query(EVENT_ARG, "num") < Query::Int(3), &found);
  ASSERT_EQ(1u, found.size());
  EXPECT_STREQ("name2", found[0]->name.c_str());

  // ||
  analyzer->FindEvents(Query(EVENT_NAME) == Query::String("name1") ||
                       Query(EVENT_ARG, "num") == Query::Int(3), &found);
  ASSERT_EQ(2u, found.size());
  EXPECT_STREQ("name1", found[0]->name.c_str());
  EXPECT_STREQ("name3", found[1]->name.c_str());

  // !
  analyzer->FindEvents(!(Query(EVENT_NAME) == Query::String("name1") ||
                         Query(EVENT_ARG, "num") == Query::Int(3)), &found);
  ASSERT_EQ(2u, found.size());
  EXPECT_STREQ("name2", found[0]->name.c_str());
  EXPECT_STREQ("name4", found[1]->name.c_str());
}

TEST_F(TraceEventAnalyzerTest, ArithmeticOperators) {
  using namespace trace_analyzer;
  ManualSetUp();

  BeginTracing();
  {
    // These events are searched for:
    TRACE_EVENT_INSTANT2("cat1", "math1", "a", 10, "b", 5);
    TRACE_EVENT_INSTANT2("cat1", "math2", "a", 10, "b", 10);
    // Extra events that never match, for noise:
    TRACE_EVENT_INSTANT2("noise", "math3", "a", 1,  "b", 3);
    TRACE_EVENT_INSTANT2("noise", "math4", "c", 10, "d", 5);
  }
  EndTracing();

  scoped_ptr<TraceAnalyzer>
      analyzer(TraceAnalyzer::Create(output_.json_output));
  ASSERT_TRUE(analyzer.get());

  TraceAnalyzer::TraceEventVector found;

  // Verify that arithmetic operators function:

  // +
  analyzer->FindEvents(Query(EVENT_ARG, "a") + Query(EVENT_ARG, "b") ==
                       Query::Int(20), &found);
  EXPECT_EQ(1u, found.size());
  EXPECT_STREQ("math2", found.front()->name.c_str());

  // -
  analyzer->FindEvents(Query(EVENT_ARG, "a") - Query(EVENT_ARG, "b") ==
                       Query::Int(5), &found);
  EXPECT_EQ(1u, found.size());
  EXPECT_STREQ("math1", found.front()->name.c_str());

  // *
  analyzer->FindEvents(Query(EVENT_ARG, "a") * Query(EVENT_ARG, "b") ==
                       Query::Int(50), &found);
  EXPECT_EQ(1u, found.size());
  EXPECT_STREQ("math1", found.front()->name.c_str());

  // /
  analyzer->FindEvents(Query(EVENT_ARG, "a") / Query(EVENT_ARG, "b") ==
                       Query::Int(2), &found);
  EXPECT_EQ(1u, found.size());
  EXPECT_STREQ("math1", found.front()->name.c_str());

  // %
  analyzer->FindEvents(Query(EVENT_ARG, "a") % Query(EVENT_ARG, "b") ==
                       Query::Int(0), &found);
  EXPECT_EQ(2u, found.size());

  // - (negate)
  analyzer->FindEvents(-Query(EVENT_ARG, "b") == Query::Int(-10), &found);
  EXPECT_EQ(1u, found.size());
  EXPECT_STREQ("math2", found.front()->name.c_str());
}

TEST_F(TraceEventAnalyzerTest, StringPattern) {
  using namespace trace_analyzer;
  ManualSetUp();

  BeginTracing();
  {
    TRACE_EVENT_INSTANT0("cat1", "name1");
    TRACE_EVENT_INSTANT0("cat1", "name2");
    TRACE_EVENT_INSTANT0("cat1", "no match");
    TRACE_EVENT_INSTANT0("cat1", "name3x");
  }
  EndTracing();

  scoped_ptr<TraceAnalyzer>
      analyzer(TraceAnalyzer::Create(output_.json_output));
  ASSERT_TRUE(analyzer.get());

  TraceAnalyzer::TraceEventVector found;

  analyzer->FindEvents(Query(EVENT_NAME) == Query::Pattern("name?"), &found);
  ASSERT_EQ(2u, found.size());
  EXPECT_STREQ("name1", found[0]->name.c_str());
  EXPECT_STREQ("name2", found[1]->name.c_str());

  analyzer->FindEvents(Query(EVENT_NAME) == Query::Pattern("name*"), &found);
  ASSERT_EQ(3u, found.size());
  EXPECT_STREQ("name1", found[0]->name.c_str());
  EXPECT_STREQ("name2", found[1]->name.c_str());
  EXPECT_STREQ("name3x", found[2]->name.c_str());

  analyzer->FindEvents(Query(EVENT_NAME) != Query::Pattern("name*"), &found);
  ASSERT_EQ(1u, found.size());
  EXPECT_STREQ("no match", found[0]->name.c_str());
}

// Test that duration queries work.
TEST_F(TraceEventAnalyzerTest, Duration) {
  using namespace trace_analyzer;
  ManualSetUp();

  int sleep_time_us = 200000;
  // We will search for events that have a duration of greater than 90% of the
  // sleep time, so that there is no flakiness.
  int duration_cutoff_us = (sleep_time_us * 9) / 10;

  BeginTracing();
  {
    TRACE_EVENT0("cat1", "name1"); // found by duration query
    TRACE_EVENT0("noise", "name2"); // not searched for, just noise
    {
      TRACE_EVENT0("cat2", "name3"); // found by duration query
      TRACE_EVENT_INSTANT0("noise", "name4"); // not searched for, just noise
      base::PlatformThread::Sleep(sleep_time_us / 1000);
      TRACE_EVENT0("cat2", "name5"); // not found (duration too short)
    }
  }
  EndTracing();

  scoped_ptr<TraceAnalyzer>
      analyzer(TraceAnalyzer::Create(output_.json_output));
  ASSERT_TRUE(analyzer.get());
  analyzer->AssociateBeginEndEvents();

  TraceAnalyzer::TraceEventVector found;
  analyzer->FindEvents(Query::MatchBeginWithEnd() &&
                      Query(EVENT_DURATION) > Query::Int(duration_cutoff_us) &&
                      (Query(EVENT_CATEGORY) == Query::String("cat1") ||
                       Query(EVENT_CATEGORY) == Query::String("cat2") ||
                       Query(EVENT_CATEGORY) == Query::String("cat3")), &found);
  ASSERT_EQ(2u, found.size());
  EXPECT_STREQ("name1", found[0]->name.c_str());
  EXPECT_STREQ("name3", found[1]->name.c_str());
}

// Test that arithmetic operators work.
TEST_F(TraceEventAnalyzerTest, BeginEndAssocations) {
  using namespace trace_analyzer;
  ManualSetUp();

  BeginTracing();
  {
    TRACE_EVENT_END0("cat1", "name1"); // does not match out of order begin
    TRACE_EVENT0("cat1", "name2");
    TRACE_EVENT_INSTANT0("cat1", "name3");
    TRACE_EVENT_BEGIN0("cat1", "name1");
  }
  EndTracing();

  scoped_ptr<TraceAnalyzer>
      analyzer(TraceAnalyzer::Create(output_.json_output));
  ASSERT_TRUE(analyzer.get());
  analyzer->AssociateBeginEndEvents();

  TraceAnalyzer::TraceEventVector found;
  analyzer->FindEvents(Query::MatchBeginWithEnd(), &found);
  ASSERT_EQ(1u, found.size());
  EXPECT_STREQ("name2", found[0]->name.c_str());
}

// Test that the TraceAnalyzer custom associations work.
TEST_F(TraceEventAnalyzerTest, CustomAssociations) {
  using namespace trace_analyzer;
  ManualSetUp();

  // Add events that begin/end in pipelined ordering with unique ID parameter
  // to match up the begin/end pairs.
  BeginTracing();
  {
    TRACE_EVENT_INSTANT1("cat1", "end", "id", 1);   // no begin match
    TRACE_EVENT_INSTANT1("cat2", "begin", "id", 2); // end is cat4
    TRACE_EVENT_INSTANT1("cat3", "begin", "id", 3); // end is cat5
    TRACE_EVENT_INSTANT1("cat4", "end", "id", 2);
    TRACE_EVENT_INSTANT1("cat5", "end", "id", 3);
    TRACE_EVENT_INSTANT1("cat6", "begin", "id", 1); // no end match
  }
  EndTracing();

  scoped_ptr<TraceAnalyzer>
      analyzer(TraceAnalyzer::Create(output_.json_output));
  ASSERT_TRUE(analyzer.get());

  // begin, end, and match queries to find proper begin/end pairs.
  Query begin(Query(EVENT_NAME) == Query::String("begin"));
  Query end(Query(EVENT_NAME) == Query::String("end"));
  Query match(Query(EVENT_ARG, "id") == Query(OTHER_ARG, "id"));
  analyzer->AssociateEvents(begin, end, match);

  TraceAnalyzer::TraceEventVector found;

  // cat1 has no other_event.
  analyzer->FindEvents(Query(EVENT_CATEGORY) == Query::String("cat1") &&
                       Query(EVENT_HAS_OTHER), &found);
  EXPECT_EQ(0u, found.size());

  // cat1 has no other_event.
  analyzer->FindEvents(Query(EVENT_CATEGORY) == Query::String("cat1") &&
                       !Query(EVENT_HAS_OTHER), &found);
  EXPECT_EQ(1u, found.size());

  // cat6 has no other_event.
  analyzer->FindEvents(Query(EVENT_CATEGORY) == Query::String("cat6") &&
                       !Query(EVENT_HAS_OTHER), &found);
  EXPECT_EQ(1u, found.size());

  // cat2 and cat4 are a associated.
  analyzer->FindEvents(Query(EVENT_CATEGORY) == Query::String("cat2") &&
                       Query(OTHER_CATEGORY) == Query::String("cat4"), &found);
  EXPECT_EQ(1u, found.size());

  // cat4 and cat2 are a associated.
  analyzer->FindEvents(Query(EVENT_CATEGORY) == Query::String("cat4") &&
                       Query(OTHER_CATEGORY) == Query::String("cat2"), &found);
  EXPECT_EQ(1u, found.size());

  // cat3 and cat5 are a associated.
  analyzer->FindEvents(Query(EVENT_CATEGORY) == Query::String("cat3") &&
                       Query(OTHER_CATEGORY) == Query::String("cat5"), &found);
  EXPECT_EQ(1u, found.size());

  // cat5 and cat3 are a associated.
  analyzer->FindEvents(Query(EVENT_CATEGORY) == Query::String("cat5") &&
                       Query(OTHER_CATEGORY) == Query::String("cat3"), &found);
  EXPECT_EQ(1u, found.size());
}

// Verify that Query literals and types are properly casted.
TEST_F(TraceEventAnalyzerTest, Literals) {
  using namespace trace_analyzer;
  ManualSetUp();

  // Since these queries don't refer to the event data, the dummy event below
  // will never be accessed.
  TraceEvent dummy;
  char char_num = 5;
  short short_num = -5;
  EXPECT_TRUE((Query::Double(5.0) == Query::Int(char_num)).Evaluate(dummy));
  EXPECT_TRUE((Query::Double(-5.0) == Query::Int(short_num)).Evaluate(dummy));
  EXPECT_TRUE((Query::Double(1.0) == Query::Uint(1u)).Evaluate(dummy));
  EXPECT_TRUE((Query::Double(1.0) == Query::Int(1)).Evaluate(dummy));
  EXPECT_TRUE((Query::Double(-1.0) == Query::Int(-1)).Evaluate(dummy));
  EXPECT_TRUE((Query::Double(1.0) == Query::Double(1.0f)).Evaluate(dummy));
  EXPECT_TRUE((Query::Bool(true) == Query::Int(1)).Evaluate(dummy));
  EXPECT_TRUE((Query::Bool(false) == Query::Int(0)).Evaluate(dummy));
  EXPECT_TRUE((Query::Bool(true) == Query::Double(1.0f)).Evaluate(dummy));
  EXPECT_TRUE((Query::Bool(false) == Query::Double(0.0f)).Evaluate(dummy));
}

// Test GetRateStats.
TEST_F(TraceEventAnalyzerTest, RateStats) {
  using namespace trace_analyzer;

  std::vector<TraceEvent> events;
  events.reserve(100);
  TraceAnalyzer::TraceEventVector event_ptrs;
  TraceEvent event;
  event.timestamp = 0.0;
  double little_delta = 1.0;
  double big_delta = 10.0;
  TraceAnalyzer::Stats stats;

  for (int i = 0; i < 10; ++i) {
    event.timestamp += little_delta;
    events.push_back(event);
    event_ptrs.push_back(&events.back());
  }

  ASSERT_TRUE(TraceAnalyzer::GetRateStats(event_ptrs, &stats));
  EXPECT_EQ(little_delta, stats.mean_us);
  EXPECT_EQ(little_delta, stats.min_us);
  EXPECT_EQ(little_delta, stats.max_us);
  EXPECT_EQ(0.0, stats.standard_deviation_us);

  event.timestamp += big_delta;
  events.push_back(event);
  event_ptrs.push_back(&events.back());

  ASSERT_TRUE(TraceAnalyzer::GetRateStats(event_ptrs, &stats));
  EXPECT_LT(little_delta, stats.mean_us);
  EXPECT_EQ(little_delta, stats.min_us);
  EXPECT_EQ(big_delta, stats.max_us);
  EXPECT_LT(0.0, stats.standard_deviation_us);

  TraceAnalyzer::TraceEventVector few_event_ptrs;
  few_event_ptrs.push_back(&event);
  few_event_ptrs.push_back(&event);
  ASSERT_FALSE(TraceAnalyzer::GetRateStats(few_event_ptrs, &stats));
  few_event_ptrs.push_back(&event);
  ASSERT_TRUE(TraceAnalyzer::GetRateStats(few_event_ptrs, &stats));
}


}  // namespace trace_analyzer

