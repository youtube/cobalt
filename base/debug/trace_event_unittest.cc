// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/debug/trace_event_unittest.h"

#include "base/bind.h"
#include "base/command_line.h"
#include "base/debug/trace_event.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/memory/ref_counted_memory.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/singleton.h"
#include "base/process_util.h"
#include "base/stringprintf.h"
#include "base/synchronization/waitable_event.h"
#include "base/threading/platform_thread.h"
#include "base/threading/thread.h"
#include "base/values.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::debug::HighResSleepForTraceTest;

namespace base {
namespace debug {

namespace {

enum CompareOp {
  IS_EQUAL,
  IS_NOT_EQUAL,
};

struct JsonKeyValue {
  const char* key;
  const char* value;
  CompareOp op;
};

class TraceEventTestFixture : public testing::Test {
 public:
  // This fixture does not use SetUp() because the fixture must be manually set
  // up multiple times when testing AtExit. Use ManualTestSetUp for this.
  void ManualTestSetUp();
  void OnTraceDataCollected(
      const scoped_refptr<base::RefCountedString>& events_str);
  void OnTraceNotification(int notification) {
    if (notification & TraceLog::EVENT_WATCH_NOTIFICATION)
      ++event_watch_notification_;
  }
  DictionaryValue* FindMatchingTraceEntry(const JsonKeyValue* key_values);
  DictionaryValue* FindNamePhase(const char* name, const char* phase);
  DictionaryValue* FindNamePhaseKeyValue(const char* name,
                                         const char* phase,
                                         const char* key,
                                         const char* value);
  bool FindMatchingValue(const char* key,
                         const char* value);
  bool FindNonMatchingValue(const char* key,
                            const char* value);
  void Clear() {
    trace_parsed_.Clear();
    json_output_.json_output.clear();
  }

  void BeginTrace() {
    event_watch_notification_ = 0;
    TraceLog::GetInstance()->SetEnabled("*");
  }

  void EndTraceAndFlush() {
    TraceLog::GetInstance()->SetDisabled();
    TraceLog::GetInstance()->Flush(
        base::Bind(&TraceEventTestFixture::OnTraceDataCollected,
                   base::Unretained(this)));
  }

  virtual void SetUp() override {
    old_thread_name_ = PlatformThread::GetName();
  }
  virtual void TearDown() override {
    if (TraceLog::GetInstance())
      EXPECT_FALSE(TraceLog::GetInstance()->IsEnabled());
    PlatformThread::SetName(old_thread_name_ ? old_thread_name_  : "");
  }

  const char* old_thread_name_;
  ListValue trace_parsed_;
  base::debug::TraceResultBuffer trace_buffer_;
  base::debug::TraceResultBuffer::SimpleOutput json_output_;
  int event_watch_notification_;

 private:
  // We want our singleton torn down after each test.
  ShadowingAtExitManager at_exit_manager_;
  Lock lock_;
};

void TraceEventTestFixture::ManualTestSetUp() {
  TraceLog::DeleteForTesting();
  TraceLog::Resurrect();
  TraceLog* tracelog = TraceLog::GetInstance();
  ASSERT_TRUE(tracelog);
  ASSERT_FALSE(tracelog->IsEnabled());
  tracelog->SetNotificationCallback(
      base::Bind(&TraceEventTestFixture::OnTraceNotification,
                 base::Unretained(this)));
  trace_buffer_.SetOutputCallback(json_output_.GetCallback());
}

void TraceEventTestFixture::OnTraceDataCollected(
    const scoped_refptr<base::RefCountedString>& events_str) {
  AutoLock lock(lock_);
  json_output_.json_output.clear();
  trace_buffer_.Start();
  trace_buffer_.AddFragment(events_str->data());
  trace_buffer_.Finish();

  scoped_ptr<Value> root;
  root.reset(base::JSONReader::Read(json_output_.json_output,
                                    JSON_PARSE_RFC | JSON_DETACHABLE_CHILDREN));

  if (!root.get()) {
    LOG(ERROR) << json_output_.json_output;
  }

  ListValue* root_list = NULL;
  ASSERT_TRUE(root.get());
  ASSERT_TRUE(root->GetAsList(&root_list));

  // Move items into our aggregate collection
  while (root_list->GetSize()) {
    Value* item = NULL;
    root_list->Remove(0, &item);
    trace_parsed_.Append(item);
  }
}

static bool CompareJsonValues(const std::string& lhs,
                              const std::string& rhs,
                              CompareOp op) {
  switch (op) {
    case IS_EQUAL:
      return lhs == rhs;
    case IS_NOT_EQUAL:
      return lhs != rhs;
    default:
      CHECK(0);
  }
  return false;
}

static bool IsKeyValueInDict(const JsonKeyValue* key_value,
                             DictionaryValue* dict) {
  Value* value = NULL;
  std::string value_str;
  if (dict->Get(key_value->key, &value) &&
      value->GetAsString(&value_str) &&
      CompareJsonValues(value_str, key_value->value, key_value->op))
    return true;

  // Recurse to test arguments
  DictionaryValue* args_dict = NULL;
  dict->GetDictionary("args", &args_dict);
  if (args_dict)
    return IsKeyValueInDict(key_value, args_dict);

  return false;
}

static bool IsAllKeyValueInDict(const JsonKeyValue* key_values,
                                DictionaryValue* dict) {
  // Scan all key_values, they must all be present and equal.
  while (key_values && key_values->key) {
    if (!IsKeyValueInDict(key_values, dict))
      return false;
    ++key_values;
  }
  return true;
}

DictionaryValue* TraceEventTestFixture::FindMatchingTraceEntry(
    const JsonKeyValue* key_values) {
  // Scan all items
  size_t trace_parsed_count = trace_parsed_.GetSize();
  for (size_t i = 0; i < trace_parsed_count; i++) {
    Value* value = NULL;
    trace_parsed_.Get(i, &value);
    if (!value || value->GetType() != Value::TYPE_DICTIONARY)
      continue;
    DictionaryValue* dict = static_cast<DictionaryValue*>(value);

    if (IsAllKeyValueInDict(key_values, dict))
      return dict;
  }
  return NULL;
}

DictionaryValue* TraceEventTestFixture::FindNamePhase(const char* name,
                                                      const char* phase) {
  JsonKeyValue key_values[] = {
    {"name", name, IS_EQUAL},
    {"ph", phase, IS_EQUAL},
    {0, 0, IS_EQUAL}
  };
  return FindMatchingTraceEntry(key_values);
}

DictionaryValue* TraceEventTestFixture::FindNamePhaseKeyValue(
    const char* name,
    const char* phase,
    const char* key,
    const char* value) {
  JsonKeyValue key_values[] = {
    {"name", name, IS_EQUAL},
    {"ph", phase, IS_EQUAL},
    {key, value, IS_EQUAL},
    {0, 0, IS_EQUAL}
  };
  return FindMatchingTraceEntry(key_values);
}

bool TraceEventTestFixture::FindMatchingValue(const char* key,
                                              const char* value) {
  JsonKeyValue key_values[] = {
    {key, value, IS_EQUAL},
    {0, 0, IS_EQUAL}
  };
  return FindMatchingTraceEntry(key_values);
}

bool TraceEventTestFixture::FindNonMatchingValue(const char* key,
                                                 const char* value) {
  JsonKeyValue key_values[] = {
    {key, value, IS_NOT_EQUAL},
    {0, 0, IS_EQUAL}
  };
  return FindMatchingTraceEntry(key_values);
}

bool IsStringInDict(const char* string_to_match, const DictionaryValue* dict) {
  for (DictionaryValue::key_iterator ikey = dict->begin_keys();
       ikey != dict->end_keys(); ++ikey) {
    const Value* child = NULL;
    if (!dict->GetWithoutPathExpansion(*ikey, &child))
      continue;

    if ((*ikey).find(string_to_match) != std::string::npos)
      return true;

    std::string value_str;
    child->GetAsString(&value_str);
    if (value_str.find(string_to_match) != std::string::npos)
      return true;
  }

  // Recurse to test arguments
  const DictionaryValue* args_dict = NULL;
  dict->GetDictionary("args", &args_dict);
  if (args_dict)
    return IsStringInDict(string_to_match, args_dict);

  return false;
}

const DictionaryValue* FindTraceEntry(
    const ListValue& trace_parsed,
    const char* string_to_match,
    const DictionaryValue* match_after_this_item = NULL) {
  // Scan all items
  size_t trace_parsed_count = trace_parsed.GetSize();
  for (size_t i = 0; i < trace_parsed_count; i++) {
    const Value* value = NULL;
    trace_parsed.Get(i, &value);
    if (match_after_this_item) {
      if (value == match_after_this_item)
         match_after_this_item = NULL;
      continue;
    }
    if (!value || value->GetType() != Value::TYPE_DICTIONARY)
      continue;
    const DictionaryValue* dict = static_cast<const DictionaryValue*>(value);

    if (IsStringInDict(string_to_match, dict))
      return dict;
  }
  return NULL;
}

std::vector<const DictionaryValue*> FindTraceEntries(
    const ListValue& trace_parsed,
    const char* string_to_match) {
  std::vector<const DictionaryValue*> hits;
  size_t trace_parsed_count = trace_parsed.GetSize();
  for (size_t i = 0; i < trace_parsed_count; i++) {
    const Value* value = NULL;
    trace_parsed.Get(i, &value);
    if (!value || value->GetType() != Value::TYPE_DICTIONARY)
      continue;
    const DictionaryValue* dict = static_cast<const DictionaryValue*>(value);

    if (IsStringInDict(string_to_match, dict))
      hits.push_back(dict);
  }
  return hits;
}

void TraceWithAllMacroVariants(WaitableEvent* task_complete_event) {
  {
    TRACE_EVENT_BEGIN_ETW("TRACE_EVENT_BEGIN_ETW call", 0x1122, "extrastring1");
    TRACE_EVENT_END_ETW("TRACE_EVENT_END_ETW call", 0x3344, "extrastring2");
    TRACE_EVENT_INSTANT_ETW("TRACE_EVENT_INSTANT_ETW call",
                            0x5566, "extrastring3");

    TRACE_EVENT0("all", "TRACE_EVENT0 call");
    TRACE_EVENT1("all", "TRACE_EVENT1 call", "name1", "value1");
    TRACE_EVENT2("all", "TRACE_EVENT2 call",
                 "name1", "\"value1\"",
                 "name2", "value\\2");

    TRACE_EVENT_INSTANT0("all", "TRACE_EVENT_INSTANT0 call");
    TRACE_EVENT_INSTANT1("all", "TRACE_EVENT_INSTANT1 call", "name1", "value1");
    TRACE_EVENT_INSTANT2("all", "TRACE_EVENT_INSTANT2 call",
                         "name1", "value1",
                         "name2", "value2");

    TRACE_EVENT_BEGIN0("all", "TRACE_EVENT_BEGIN0 call");
    TRACE_EVENT_BEGIN1("all", "TRACE_EVENT_BEGIN1 call", "name1", "value1");
    TRACE_EVENT_BEGIN2("all", "TRACE_EVENT_BEGIN2 call",
                       "name1", "value1",
                       "name2", "value2");

    TRACE_EVENT_END0("all", "TRACE_EVENT_END0 call");
    TRACE_EVENT_END1("all", "TRACE_EVENT_END1 call", "name1", "value1");
    TRACE_EVENT_END2("all", "TRACE_EVENT_END2 call",
                     "name1", "value1",
                     "name2", "value2");

    TRACE_EVENT_ASYNC_BEGIN0("all", "TRACE_EVENT_ASYNC_BEGIN0 call", 5);
    TRACE_EVENT_ASYNC_BEGIN1("all", "TRACE_EVENT_ASYNC_BEGIN1 call", 5,
                             "name1", "value1");
    TRACE_EVENT_ASYNC_BEGIN2("all", "TRACE_EVENT_ASYNC_BEGIN2 call", 5,
                             "name1", "value1",
                             "name2", "value2");

    TRACE_EVENT_ASYNC_STEP0("all", "TRACE_EVENT_ASYNC_STEP0 call",
                                  5, "step1");
    TRACE_EVENT_ASYNC_STEP1("all", "TRACE_EVENT_ASYNC_STEP1 call",
                                  5, "step2", "name1", "value1");

    TRACE_EVENT_ASYNC_END0("all", "TRACE_EVENT_ASYNC_END0 call", 5);
    TRACE_EVENT_ASYNC_END1("all", "TRACE_EVENT_ASYNC_END1 call", 5,
                           "name1", "value1");
    TRACE_EVENT_ASYNC_END2("all", "TRACE_EVENT_ASYNC_END2 call", 5,
                           "name1", "value1",
                           "name2", "value2");

    TRACE_EVENT_BEGIN_ETW("TRACE_EVENT_BEGIN_ETW0 call", 5, NULL);
    TRACE_EVENT_BEGIN_ETW("TRACE_EVENT_BEGIN_ETW1 call", 5, "value");
    TRACE_EVENT_END_ETW("TRACE_EVENT_END_ETW0 call", 5, NULL);
    TRACE_EVENT_END_ETW("TRACE_EVENT_END_ETW1 call", 5, "value");
    TRACE_EVENT_INSTANT_ETW("TRACE_EVENT_INSTANT_ETW0 call", 5, NULL);
    TRACE_EVENT_INSTANT_ETW("TRACE_EVENT_INSTANT_ETW1 call", 5, "value");

    TRACE_COUNTER1("all", "TRACE_COUNTER1 call", 31415);
    TRACE_COUNTER2("all", "TRACE_COUNTER2 call",
                   "a", 30000,
                   "b", 1415);

    TRACE_COUNTER_ID1("all", "TRACE_COUNTER_ID1 call", 0x319009, 31415);
    TRACE_COUNTER_ID2("all", "TRACE_COUNTER_ID2 call", 0x319009,
                      "a", 30000, "b", 1415);
  } // Scope close causes TRACE_EVENT0 etc to send their END events.

  if (task_complete_event)
    task_complete_event->Signal();
}

void ValidateAllTraceMacrosCreatedData(const ListValue& trace_parsed) {
  const DictionaryValue* item = NULL;

#define EXPECT_FIND_(string) \
    EXPECT_TRUE((item = FindTraceEntry(trace_parsed, string)));
#define EXPECT_NOT_FIND_(string) \
    EXPECT_FALSE((item = FindTraceEntry(trace_parsed, string)));
#define EXPECT_SUB_FIND_(string) \
    if (item) EXPECT_TRUE((IsStringInDict(string, item)));

  EXPECT_FIND_("ETW Trace Event");
  EXPECT_FIND_("all");
  EXPECT_FIND_("TRACE_EVENT_BEGIN_ETW call");
  {
    std::string str_val;
    EXPECT_TRUE(item && item->GetString("args.id", &str_val));
    EXPECT_STREQ("1122", str_val.c_str());
  }
  EXPECT_SUB_FIND_("extrastring1");
  EXPECT_FIND_("TRACE_EVENT_END_ETW call");
  EXPECT_FIND_("TRACE_EVENT_INSTANT_ETW call");
  EXPECT_FIND_("TRACE_EVENT0 call");
  {
    std::string ph_begin;
    std::string ph_end;
    EXPECT_TRUE((item = FindTraceEntry(trace_parsed, "TRACE_EVENT0 call")));
    EXPECT_TRUE((item && item->GetString("ph", &ph_begin)));
    EXPECT_TRUE((item = FindTraceEntry(trace_parsed, "TRACE_EVENT0 call",
                                       item)));
    EXPECT_TRUE((item && item->GetString("ph", &ph_end)));
    EXPECT_EQ("B", ph_begin);
    EXPECT_EQ("E", ph_end);
  }
  EXPECT_FIND_("TRACE_EVENT1 call");
  EXPECT_SUB_FIND_("name1");
  EXPECT_SUB_FIND_("value1");
  EXPECT_FIND_("TRACE_EVENT2 call");
  EXPECT_SUB_FIND_("name1");
  EXPECT_SUB_FIND_("\"value1\"");
  EXPECT_SUB_FIND_("name2");
  EXPECT_SUB_FIND_("value\\2");

  EXPECT_FIND_("TRACE_EVENT_INSTANT0 call");
  EXPECT_FIND_("TRACE_EVENT_INSTANT1 call");
  EXPECT_SUB_FIND_("name1");
  EXPECT_SUB_FIND_("value1");
  EXPECT_FIND_("TRACE_EVENT_INSTANT2 call");
  EXPECT_SUB_FIND_("name1");
  EXPECT_SUB_FIND_("value1");
  EXPECT_SUB_FIND_("name2");
  EXPECT_SUB_FIND_("value2");

  EXPECT_FIND_("TRACE_EVENT_BEGIN0 call");
  EXPECT_FIND_("TRACE_EVENT_BEGIN1 call");
  EXPECT_SUB_FIND_("name1");
  EXPECT_SUB_FIND_("value1");
  EXPECT_FIND_("TRACE_EVENT_BEGIN2 call");
  EXPECT_SUB_FIND_("name1");
  EXPECT_SUB_FIND_("value1");
  EXPECT_SUB_FIND_("name2");
  EXPECT_SUB_FIND_("value2");

  EXPECT_FIND_("TRACE_EVENT_END0 call");
  EXPECT_FIND_("TRACE_EVENT_END1 call");
  EXPECT_SUB_FIND_("name1");
  EXPECT_SUB_FIND_("value1");
  EXPECT_FIND_("TRACE_EVENT_END2 call");
  EXPECT_SUB_FIND_("name1");
  EXPECT_SUB_FIND_("value1");
  EXPECT_SUB_FIND_("name2");
  EXPECT_SUB_FIND_("value2");

  EXPECT_FIND_("TRACE_EVENT_ASYNC_BEGIN0 call");
  EXPECT_SUB_FIND_("id");
  EXPECT_SUB_FIND_("5");
  EXPECT_FIND_("TRACE_EVENT_ASYNC_BEGIN1 call");
  EXPECT_SUB_FIND_("id");
  EXPECT_SUB_FIND_("5");
  EXPECT_SUB_FIND_("name1");
  EXPECT_SUB_FIND_("value1");
  EXPECT_FIND_("TRACE_EVENT_ASYNC_BEGIN2 call");
  EXPECT_SUB_FIND_("id");
  EXPECT_SUB_FIND_("5");
  EXPECT_SUB_FIND_("name1");
  EXPECT_SUB_FIND_("value1");
  EXPECT_SUB_FIND_("name2");
  EXPECT_SUB_FIND_("value2");

  EXPECT_FIND_("TRACE_EVENT_ASYNC_STEP0 call");
  EXPECT_SUB_FIND_("id");
  EXPECT_SUB_FIND_("5");
  EXPECT_SUB_FIND_("step1");
  EXPECT_FIND_("TRACE_EVENT_ASYNC_STEP1 call");
  EXPECT_SUB_FIND_("id");
  EXPECT_SUB_FIND_("5");
  EXPECT_SUB_FIND_("step2");
  EXPECT_SUB_FIND_("name1");
  EXPECT_SUB_FIND_("value1");

  EXPECT_FIND_("TRACE_EVENT_ASYNC_END0 call");
  EXPECT_SUB_FIND_("id");
  EXPECT_SUB_FIND_("5");
  EXPECT_FIND_("TRACE_EVENT_ASYNC_END1 call");
  EXPECT_SUB_FIND_("id");
  EXPECT_SUB_FIND_("5");
  EXPECT_SUB_FIND_("name1");
  EXPECT_SUB_FIND_("value1");
  EXPECT_FIND_("TRACE_EVENT_ASYNC_END2 call");
  EXPECT_SUB_FIND_("id");
  EXPECT_SUB_FIND_("5");
  EXPECT_SUB_FIND_("name1");
  EXPECT_SUB_FIND_("value1");
  EXPECT_SUB_FIND_("name2");
  EXPECT_SUB_FIND_("value2");

  EXPECT_FIND_("TRACE_EVENT_BEGIN_ETW0 call");
  EXPECT_SUB_FIND_("id");
  EXPECT_SUB_FIND_("5");
  EXPECT_SUB_FIND_("extra");
  EXPECT_SUB_FIND_("NULL");
  EXPECT_FIND_("TRACE_EVENT_BEGIN_ETW1 call");
  EXPECT_SUB_FIND_("id");
  EXPECT_SUB_FIND_("5");
  EXPECT_SUB_FIND_("extra");
  EXPECT_SUB_FIND_("value");
  EXPECT_FIND_("TRACE_EVENT_END_ETW0 call");
  EXPECT_SUB_FIND_("id");
  EXPECT_SUB_FIND_("5");
  EXPECT_SUB_FIND_("extra");
  EXPECT_SUB_FIND_("NULL");
  EXPECT_FIND_("TRACE_EVENT_END_ETW1 call");
  EXPECT_SUB_FIND_("id");
  EXPECT_SUB_FIND_("5");
  EXPECT_SUB_FIND_("extra");
  EXPECT_SUB_FIND_("value");
  EXPECT_FIND_("TRACE_EVENT_INSTANT_ETW0 call");
  EXPECT_SUB_FIND_("id");
  EXPECT_SUB_FIND_("5");
  EXPECT_SUB_FIND_("extra");
  EXPECT_SUB_FIND_("NULL");
  EXPECT_FIND_("TRACE_EVENT_INSTANT_ETW1 call");
  EXPECT_SUB_FIND_("id");
  EXPECT_SUB_FIND_("5");
  EXPECT_SUB_FIND_("extra");
  EXPECT_SUB_FIND_("value");

  EXPECT_FIND_("TRACE_COUNTER1 call");
  {
    std::string ph;
    EXPECT_TRUE((item && item->GetString("ph", &ph)));
    EXPECT_EQ("C", ph);

    int value;
    EXPECT_TRUE((item && item->GetInteger("args.value", &value)));
    EXPECT_EQ(31415, value);
  }

  EXPECT_FIND_("TRACE_COUNTER2 call");
  {
    std::string ph;
    EXPECT_TRUE((item && item->GetString("ph", &ph)));
    EXPECT_EQ("C", ph);

    int value;
    EXPECT_TRUE((item && item->GetInteger("args.a", &value)));
    EXPECT_EQ(30000, value);

    EXPECT_TRUE((item && item->GetInteger("args.b", &value)));
    EXPECT_EQ(1415, value);
  }

  EXPECT_FIND_("TRACE_COUNTER_ID1 call");
  {
    std::string id;
    EXPECT_TRUE((item && item->GetString("id", &id)));
    EXPECT_EQ("319009", id);

    std::string ph;
    EXPECT_TRUE((item && item->GetString("ph", &ph)));
    EXPECT_EQ("C", ph);

    int value;
    EXPECT_TRUE((item && item->GetInteger("args.value", &value)));
    EXPECT_EQ(31415, value);
  }

  EXPECT_FIND_("TRACE_COUNTER_ID2 call");
  {
    std::string id;
    EXPECT_TRUE((item && item->GetString("id", &id)));
    EXPECT_EQ("319009", id);

    std::string ph;
    EXPECT_TRUE((item && item->GetString("ph", &ph)));
    EXPECT_EQ("C", ph);

    int value;
    EXPECT_TRUE((item && item->GetInteger("args.a", &value)));
    EXPECT_EQ(30000, value);

    EXPECT_TRUE((item && item->GetInteger("args.b", &value)));
    EXPECT_EQ(1415, value);
  }
}

void TraceManyInstantEvents(int thread_id, int num_events,
                            WaitableEvent* task_complete_event) {
  for (int i = 0; i < num_events; i++) {
    TRACE_EVENT_INSTANT2("all", "multi thread event",
                         "thread", thread_id,
                         "event", i);
  }

  if (task_complete_event)
    task_complete_event->Signal();
}

void ValidateInstantEventPresentOnEveryThread(const ListValue& trace_parsed,
                                              int num_threads,
                                              int num_events) {
  std::map<int, std::map<int, bool> > results;

  size_t trace_parsed_count = trace_parsed.GetSize();
  for (size_t i = 0; i < trace_parsed_count; i++) {
    const Value* value = NULL;
    trace_parsed.Get(i, &value);
    if (!value || value->GetType() != Value::TYPE_DICTIONARY)
      continue;
    const DictionaryValue* dict = static_cast<const DictionaryValue*>(value);
    std::string name;
    dict->GetString("name", &name);
    if (name != "multi thread event")
      continue;

    int thread = 0;
    int event = 0;
    EXPECT_TRUE(dict->GetInteger("args.thread", &thread));
    EXPECT_TRUE(dict->GetInteger("args.event", &event));
    results[thread][event] = true;
  }

  EXPECT_FALSE(results[-1][-1]);
  for (int thread = 0; thread < num_threads; thread++) {
    for (int event = 0; event < num_events; event++) {
      EXPECT_TRUE(results[thread][event]);
    }
  }
}

void TraceCallsWithCachedCategoryPointersPointers(const char* name_str) {
  TRACE_EVENT0("category name1", name_str);
  TRACE_EVENT_INSTANT0("category name2", name_str);
  TRACE_EVENT_BEGIN0("category name3", name_str);
  TRACE_EVENT_END0("category name4", name_str);
}

}  // namespace

void HighResSleepForTraceTest(base::TimeDelta elapsed) {
  base::TimeTicks end_time = base::TimeTicks::HighResNow() + elapsed;
  do {
    base::PlatformThread::Sleep(base::TimeDelta::FromMilliseconds(1));
  } while (base::TimeTicks::HighResNow() < end_time);
}

// Simple Test for emitting data and validating it was received.
TEST_F(TraceEventTestFixture, DataCaptured) {
  ManualTestSetUp();
  TraceLog::GetInstance()->SetEnabled(true);

  TraceWithAllMacroVariants(NULL);

  EndTraceAndFlush();

  ValidateAllTraceMacrosCreatedData(trace_parsed_);
}

class MockEnabledStateChangedObserver :
      public base::debug::TraceLog::EnabledStateChangedObserver {
 public:
  MOCK_METHOD0(OnTraceLogWillEnable, void());
  MOCK_METHOD0(OnTraceLogWillDisable, void());
};

TEST_F(TraceEventTestFixture, EnabledObserverFiresOnEnable) {
  ManualTestSetUp();

  MockEnabledStateChangedObserver observer;
  TraceLog::GetInstance()->AddEnabledStateObserver(&observer);

  EXPECT_CALL(observer, OnTraceLogWillEnable())
      .Times(1);
  TraceLog::GetInstance()->SetEnabled(true);
  testing::Mock::VerifyAndClear(&observer);

  // Cleanup.
  TraceLog::GetInstance()->RemoveEnabledStateObserver(&observer);
  TraceLog::GetInstance()->SetEnabled(false);
}

TEST_F(TraceEventTestFixture, EnabledObserverDoesntFireOnSecondEnable) {
  ManualTestSetUp();

  TraceLog::GetInstance()->SetEnabled(true);

  testing::StrictMock<MockEnabledStateChangedObserver> observer;
  TraceLog::GetInstance()->AddEnabledStateObserver(&observer);

  EXPECT_CALL(observer, OnTraceLogWillEnable())
      .Times(0);
  EXPECT_CALL(observer, OnTraceLogWillDisable())
      .Times(0);
  TraceLog::GetInstance()->SetEnabled(true);
  testing::Mock::VerifyAndClear(&observer);

  // Cleanup.
  TraceLog::GetInstance()->RemoveEnabledStateObserver(&observer);
  TraceLog::GetInstance()->SetEnabled(false);
}

TEST_F(TraceEventTestFixture, EnabledObserverDoesntFireOnUselessDisable) {
  ManualTestSetUp();


  testing::StrictMock<MockEnabledStateChangedObserver> observer;
  TraceLog::GetInstance()->AddEnabledStateObserver(&observer);

  EXPECT_CALL(observer, OnTraceLogWillEnable())
      .Times(0);
  EXPECT_CALL(observer, OnTraceLogWillDisable())
      .Times(0);
  TraceLog::GetInstance()->SetEnabled(false);
  testing::Mock::VerifyAndClear(&observer);

  // Cleanup.
  TraceLog::GetInstance()->RemoveEnabledStateObserver(&observer);
}

TEST_F(TraceEventTestFixture, EnabledObserverFiresOnDisable) {
  ManualTestSetUp();

  TraceLog::GetInstance()->SetEnabled(true);

  MockEnabledStateChangedObserver observer;
  TraceLog::GetInstance()->AddEnabledStateObserver(&observer);

  EXPECT_CALL(observer, OnTraceLogWillDisable())
      .Times(1);
  TraceLog::GetInstance()->SetEnabled(false);
  testing::Mock::VerifyAndClear(&observer);

  // Cleanup.
  TraceLog::GetInstance()->RemoveEnabledStateObserver(&observer);
}

// Test that categories work.
TEST_F(TraceEventTestFixture, Categories) {
  ManualTestSetUp();

  // Test that categories that are used can be retrieved whether trace was
  // enabled or disabled when the trace event was encountered.
  TRACE_EVENT_INSTANT0("c1", "name");
  TRACE_EVENT_INSTANT0("c2", "name");
  BeginTrace();
  TRACE_EVENT_INSTANT0("c3", "name");
  TRACE_EVENT_INSTANT0("c4", "name");
  EndTraceAndFlush();
  std::vector<std::string> cats;
  TraceLog::GetInstance()->GetKnownCategories(&cats);
  EXPECT_TRUE(std::find(cats.begin(), cats.end(), "c1") != cats.end());
  EXPECT_TRUE(std::find(cats.begin(), cats.end(), "c2") != cats.end());
  EXPECT_TRUE(std::find(cats.begin(), cats.end(), "c3") != cats.end());
  EXPECT_TRUE(std::find(cats.begin(), cats.end(), "c4") != cats.end());

  const std::vector<std::string> empty_categories;
  std::vector<std::string> included_categories;
  std::vector<std::string> excluded_categories;

  // Test that category filtering works.

  // Include nonexistent category -> no events
  Clear();
  included_categories.clear();
  included_categories.push_back("not_found823564786");
  TraceLog::GetInstance()->SetEnabled(included_categories, empty_categories);
  TRACE_EVENT_INSTANT0("cat1", "name");
  TRACE_EVENT_INSTANT0("cat2", "name");
  EndTraceAndFlush();
  EXPECT_TRUE(trace_parsed_.empty());

  // Include existent category -> only events of that category
  Clear();
  included_categories.clear();
  included_categories.push_back("inc");
  TraceLog::GetInstance()->SetEnabled(included_categories, empty_categories);
  TRACE_EVENT_INSTANT0("inc", "name");
  TRACE_EVENT_INSTANT0("inc2", "name");
  EndTraceAndFlush();
  EXPECT_TRUE(FindMatchingValue("cat", "inc"));
  EXPECT_FALSE(FindNonMatchingValue("cat", "inc"));

  // Include existent wildcard -> all categories matching wildcard
  Clear();
  included_categories.clear();
  included_categories.push_back("inc_wildcard_*");
  included_categories.push_back("inc_wildchar_?_end");
  TraceLog::GetInstance()->SetEnabled(included_categories, empty_categories);
  TRACE_EVENT_INSTANT0("inc_wildcard_abc", "included");
  TRACE_EVENT_INSTANT0("inc_wildcard_", "included");
  TRACE_EVENT_INSTANT0("inc_wildchar_x_end", "included");
  TRACE_EVENT_INSTANT0("inc_wildchar_bla_end", "not_inc");
  TRACE_EVENT_INSTANT0("cat1", "not_inc");
  TRACE_EVENT_INSTANT0("cat2", "not_inc");
  EndTraceAndFlush();
  EXPECT_TRUE(FindMatchingValue("cat", "inc_wildcard_abc"));
  EXPECT_TRUE(FindMatchingValue("cat", "inc_wildcard_"));
  EXPECT_TRUE(FindMatchingValue("cat", "inc_wildchar_x_end"));
  EXPECT_FALSE(FindMatchingValue("name", "not_inc"));

  included_categories.clear();

  // Exclude nonexistent category -> all events
  Clear();
  excluded_categories.clear();
  excluded_categories.push_back("not_found823564786");
  TraceLog::GetInstance()->SetEnabled(empty_categories, excluded_categories);
  TRACE_EVENT_INSTANT0("cat1", "name");
  TRACE_EVENT_INSTANT0("cat2", "name");
  EndTraceAndFlush();
  EXPECT_TRUE(FindMatchingValue("cat", "cat1"));
  EXPECT_TRUE(FindMatchingValue("cat", "cat2"));

  // Exclude existent category -> only events of other categories
  Clear();
  excluded_categories.clear();
  excluded_categories.push_back("inc");
  TraceLog::GetInstance()->SetEnabled(empty_categories, excluded_categories);
  TRACE_EVENT_INSTANT0("inc", "name");
  TRACE_EVENT_INSTANT0("inc2", "name");
  EndTraceAndFlush();
  EXPECT_TRUE(FindMatchingValue("cat", "inc2"));
  EXPECT_FALSE(FindMatchingValue("cat", "inc"));

  // Exclude existent wildcard -> all categories not matching wildcard
  Clear();
  excluded_categories.clear();
  excluded_categories.push_back("inc_wildcard_*");
  excluded_categories.push_back("inc_wildchar_?_end");
  TraceLog::GetInstance()->SetEnabled(empty_categories, excluded_categories);
  TRACE_EVENT_INSTANT0("inc_wildcard_abc", "not_inc");
  TRACE_EVENT_INSTANT0("inc_wildcard_", "not_inc");
  TRACE_EVENT_INSTANT0("inc_wildchar_x_end", "not_inc");
  TRACE_EVENT_INSTANT0("inc_wildchar_bla_end", "included");
  TRACE_EVENT_INSTANT0("cat1", "included");
  TRACE_EVENT_INSTANT0("cat2", "included");
  EndTraceAndFlush();
  EXPECT_TRUE(FindMatchingValue("cat", "inc_wildchar_bla_end"));
  EXPECT_TRUE(FindMatchingValue("cat", "cat1"));
  EXPECT_TRUE(FindMatchingValue("cat", "cat2"));
  EXPECT_FALSE(FindMatchingValue("name", "not_inc"));
}


// Test EVENT_WATCH_NOTIFICATION
TEST_F(TraceEventTestFixture, EventWatchNotification) {
  ManualTestSetUp();

  // Basic one occurrence.
  BeginTrace();
  TraceLog::GetInstance()->SetWatchEvent("cat", "event");
  TRACE_EVENT_INSTANT0("cat", "event");
  EndTraceAndFlush();
  EXPECT_EQ(event_watch_notification_, 1);

  // Basic one occurrence before Set.
  BeginTrace();
  TRACE_EVENT_INSTANT0("cat", "event");
  TraceLog::GetInstance()->SetWatchEvent("cat", "event");
  EndTraceAndFlush();
  EXPECT_EQ(event_watch_notification_, 1);

  // Auto-reset after end trace.
  BeginTrace();
  TraceLog::GetInstance()->SetWatchEvent("cat", "event");
  EndTraceAndFlush();
  BeginTrace();
  TRACE_EVENT_INSTANT0("cat", "event");
  EndTraceAndFlush();
  EXPECT_EQ(event_watch_notification_, 0);

  // Multiple occurrence.
  BeginTrace();
  int num_occurrences = 5;
  TraceLog::GetInstance()->SetWatchEvent("cat", "event");
  for (int i = 0; i < num_occurrences; ++i)
    TRACE_EVENT_INSTANT0("cat", "event");
  EndTraceAndFlush();
  EXPECT_EQ(event_watch_notification_, num_occurrences);

  // Wrong category.
  BeginTrace();
  TraceLog::GetInstance()->SetWatchEvent("cat", "event");
  TRACE_EVENT_INSTANT0("wrong_cat", "event");
  EndTraceAndFlush();
  EXPECT_EQ(event_watch_notification_, 0);

  // Wrong name.
  BeginTrace();
  TraceLog::GetInstance()->SetWatchEvent("cat", "event");
  TRACE_EVENT_INSTANT0("cat", "wrong_event");
  EndTraceAndFlush();
  EXPECT_EQ(event_watch_notification_, 0);

  // Canceled.
  BeginTrace();
  TraceLog::GetInstance()->SetWatchEvent("cat", "event");
  TraceLog::GetInstance()->CancelWatchEvent();
  TRACE_EVENT_INSTANT0("cat", "event");
  EndTraceAndFlush();
  EXPECT_EQ(event_watch_notification_, 0);
}

// Test ASYNC_BEGIN/END events
TEST_F(TraceEventTestFixture, AsyncBeginEndEvents) {
  ManualTestSetUp();
  BeginTrace();

  unsigned long long id = 0xfeedbeeffeedbeefull;
  TRACE_EVENT_ASYNC_BEGIN0( "cat", "name1", id);
  TRACE_EVENT_ASYNC_STEP0( "cat", "name1", id, "step1");
  TRACE_EVENT_ASYNC_END0("cat", "name1", id);
  TRACE_EVENT_BEGIN0( "cat", "name2");
  TRACE_EVENT_ASYNC_BEGIN0( "cat", "name3", 0);

  EndTraceAndFlush();

  EXPECT_TRUE(FindNamePhase("name1", "S"));
  EXPECT_TRUE(FindNamePhase("name1", "T"));
  EXPECT_TRUE(FindNamePhase("name1", "F"));

  std::string id_str;
  StringAppendF(&id_str, "%llx", id);

  EXPECT_TRUE(FindNamePhaseKeyValue("name1", "S", "id", id_str.c_str()));
  EXPECT_TRUE(FindNamePhaseKeyValue("name1", "T", "id", id_str.c_str()));
  EXPECT_TRUE(FindNamePhaseKeyValue("name1", "F", "id", id_str.c_str()));
  EXPECT_TRUE(FindNamePhaseKeyValue("name3", "S", "id", "0"));

  // BEGIN events should not have id
  EXPECT_FALSE(FindNamePhaseKeyValue("name2", "B", "id", "0"));
}

// Test ASYNC_BEGIN/END events
TEST_F(TraceEventTestFixture, AsyncBeginEndPointerMangling) {
  ManualTestSetUp();

  void* ptr = this;

  TraceLog::GetInstance()->SetProcessID(100);
  BeginTrace();
  TRACE_EVENT_ASYNC_BEGIN0( "cat", "name1", ptr);
  TRACE_EVENT_ASYNC_BEGIN0( "cat", "name2", ptr);
  EndTraceAndFlush();

  TraceLog::GetInstance()->SetProcessID(200);
  BeginTrace();
  TRACE_EVENT_ASYNC_END0( "cat", "name1", ptr);
  EndTraceAndFlush();

  DictionaryValue* async_begin = FindNamePhase("name1", "S");
  DictionaryValue* async_begin2 = FindNamePhase("name2", "S");
  DictionaryValue* async_end = FindNamePhase("name1", "F");
  EXPECT_TRUE(async_begin);
  EXPECT_TRUE(async_begin2);
  EXPECT_TRUE(async_end);

  Value* value = NULL;
  std::string async_begin_id_str;
  std::string async_begin2_id_str;
  std::string async_end_id_str;
  ASSERT_TRUE(async_begin->Get("id", &value));
  ASSERT_TRUE(value->GetAsString(&async_begin_id_str));
  ASSERT_TRUE(async_begin2->Get("id", &value));
  ASSERT_TRUE(value->GetAsString(&async_begin2_id_str));
  ASSERT_TRUE(async_end->Get("id", &value));
  ASSERT_TRUE(value->GetAsString(&async_end_id_str));

  EXPECT_STREQ(async_begin_id_str.c_str(), async_begin2_id_str.c_str());
  EXPECT_STRNE(async_begin_id_str.c_str(), async_end_id_str.c_str());
}

// Test that static strings are not copied.
TEST_F(TraceEventTestFixture, StaticStringVsString) {
  ManualTestSetUp();
  TraceLog* tracer = TraceLog::GetInstance();
  // Make sure old events are flushed:
  EndTraceAndFlush();
  EXPECT_EQ(0u, tracer->GetEventsSize());

  {
    BeginTrace();
    // Test that string arguments are copied.
    TRACE_EVENT2("cat", "name1",
                 "arg1", std::string("argval"), "arg2", std::string("argval"));
    // Test that static TRACE_STR_COPY string arguments are copied.
    TRACE_EVENT2("cat", "name2",
                 "arg1", TRACE_STR_COPY("argval"),
                 "arg2", TRACE_STR_COPY("argval"));
    size_t num_events = tracer->GetEventsSize();
    EXPECT_GT(num_events, 1u);
    const TraceEvent& event1 = tracer->GetEventAt(num_events - 2);
    const TraceEvent& event2 = tracer->GetEventAt(num_events - 1);
    EXPECT_STREQ("name1", event1.name());
    EXPECT_STREQ("name2", event2.name());
    EXPECT_TRUE(event1.parameter_copy_storage() != NULL);
    EXPECT_TRUE(event2.parameter_copy_storage() != NULL);
    EXPECT_GT(event1.parameter_copy_storage()->size(), 0u);
    EXPECT_GT(event2.parameter_copy_storage()->size(), 0u);
    EndTraceAndFlush();
  }

  {
    BeginTrace();
    // Test that static literal string arguments are not copied.
    TRACE_EVENT2("cat", "name1",
                 "arg1", "argval", "arg2", "argval");
    // Test that static TRACE_STR_COPY NULL string arguments are not copied.
    const char* str1 = NULL;
    const char* str2 = NULL;
    TRACE_EVENT2("cat", "name2",
                 "arg1", TRACE_STR_COPY(str1),
                 "arg2", TRACE_STR_COPY(str2));
    size_t num_events = tracer->GetEventsSize();
    EXPECT_GT(num_events, 1u);
    const TraceEvent& event1 = tracer->GetEventAt(num_events - 2);
    const TraceEvent& event2 = tracer->GetEventAt(num_events - 1);
    EXPECT_STREQ("name1", event1.name());
    EXPECT_STREQ("name2", event2.name());
    EXPECT_TRUE(event1.parameter_copy_storage() == NULL);
    EXPECT_TRUE(event2.parameter_copy_storage() == NULL);
    EndTraceAndFlush();
  }
}

// Test that data sent from other threads is gathered
TEST_F(TraceEventTestFixture, DataCapturedOnThread) {
  ManualTestSetUp();
  BeginTrace();

  Thread thread("1");
  WaitableEvent task_complete_event(false, false);
  thread.Start();

  thread.message_loop()->PostTask(
      FROM_HERE, base::Bind(&TraceWithAllMacroVariants, &task_complete_event));
  task_complete_event.Wait();
  thread.Stop();

  EndTraceAndFlush();
  ValidateAllTraceMacrosCreatedData(trace_parsed_);
}

// Test that data sent from multiple threads is gathered
TEST_F(TraceEventTestFixture, DataCapturedManyThreads) {
  ManualTestSetUp();
  BeginTrace();

  const int num_threads = 4;
  const int num_events = 4000;
  Thread* threads[num_threads];
  WaitableEvent* task_complete_events[num_threads];
  for (int i = 0; i < num_threads; i++) {
    threads[i] = new Thread(StringPrintf("Thread %d", i).c_str());
    task_complete_events[i] = new WaitableEvent(false, false);
    threads[i]->Start();
    threads[i]->message_loop()->PostTask(
        FROM_HERE, base::Bind(&TraceManyInstantEvents,
                              i, num_events, task_complete_events[i]));
  }

  for (int i = 0; i < num_threads; i++) {
    task_complete_events[i]->Wait();
  }

  for (int i = 0; i < num_threads; i++) {
    threads[i]->Stop();
    delete threads[i];
    delete task_complete_events[i];
  }

  EndTraceAndFlush();

  ValidateInstantEventPresentOnEveryThread(trace_parsed_,
                                           num_threads, num_events);
}

// Test that thread and process names show up in the trace
TEST_F(TraceEventTestFixture, ThreadNames) {
  ManualTestSetUp();

  // Create threads before we enable tracing to make sure
  // that tracelog still captures them.
  const int num_threads = 4;
  const int num_events = 10;
  Thread* threads[num_threads];
  PlatformThreadId thread_ids[num_threads];
  for (int i = 0; i < num_threads; i++)
    threads[i] = new Thread(StringPrintf("Thread %d", i).c_str());

  // Enable tracing.
  BeginTrace();

  // Now run some trace code on these threads.
  WaitableEvent* task_complete_events[num_threads];
  for (int i = 0; i < num_threads; i++) {
    task_complete_events[i] = new WaitableEvent(false, false);
    threads[i]->Start();
    thread_ids[i] = threads[i]->thread_id();
    threads[i]->message_loop()->PostTask(
        FROM_HERE, base::Bind(&TraceManyInstantEvents,
                              i, num_events, task_complete_events[i]));
  }
  for (int i = 0; i < num_threads; i++) {
    task_complete_events[i]->Wait();
  }

  // Shut things down.
  for (int i = 0; i < num_threads; i++) {
    threads[i]->Stop();
    delete threads[i];
    delete task_complete_events[i];
  }

  EndTraceAndFlush();

  std::string tmp;
  int tmp_int;
  const DictionaryValue* item;

  // Make sure we get thread name metadata.
  // Note, the test suite may have created a ton of threads.
  // So, we'll have thread names for threads we didn't create.
  std::vector<const DictionaryValue*> items =
      FindTraceEntries(trace_parsed_, "thread_name");
  for (int i = 0; i < static_cast<int>(items.size()); i++) {
    item = items[i];
    ASSERT_TRUE(item);
    EXPECT_TRUE(item->GetInteger("tid", &tmp_int));

    // See if this thread name is one of the threads we just created
    for (int j = 0; j < num_threads; j++) {
      if(static_cast<int>(thread_ids[j]) != tmp_int)
        continue;

      std::string expected_name = StringPrintf("Thread %d", j);
      EXPECT_TRUE(item->GetString("ph", &tmp) && tmp == "M");
      EXPECT_TRUE(item->GetInteger("pid", &tmp_int) &&
                  tmp_int == static_cast<int>(base::GetCurrentProcId()));
      // If the thread name changes or the tid gets reused, the name will be
      // a comma-separated list of thread names, so look for a substring.
      EXPECT_TRUE(item->GetString("args.name", &tmp) &&
                  tmp.find(expected_name) != std::string::npos);
    }
  }
}

TEST_F(TraceEventTestFixture, ThreadNameChanges) {
  ManualTestSetUp();

  BeginTrace();

  PlatformThread::SetName("");
  TRACE_EVENT_INSTANT0("drink", "water");

  PlatformThread::SetName("cafe");
  TRACE_EVENT_INSTANT0("drink", "coffee");

  PlatformThread::SetName("shop");
  // No event here, so won't appear in combined name.

  PlatformThread::SetName("pub");
  TRACE_EVENT_INSTANT0("drink", "beer");
  TRACE_EVENT_INSTANT0("drink", "wine");

  PlatformThread::SetName(" bar");
  TRACE_EVENT_INSTANT0("drink", "whisky");

  EndTraceAndFlush();

  std::vector<const DictionaryValue*> items =
      FindTraceEntries(trace_parsed_, "thread_name");
  EXPECT_EQ(1u, items.size());
  ASSERT_GT(items.size(), 0u);
  const DictionaryValue* item = items[0];
  ASSERT_TRUE(item);
  int tid;
  EXPECT_TRUE(item->GetInteger("tid", &tid));
  EXPECT_EQ(PlatformThread::CurrentId(), static_cast<PlatformThreadId>(tid));

  std::string expected_name = "cafe,pub, bar";
  std::string tmp;
  EXPECT_TRUE(item->GetString("args.name", &tmp));
  EXPECT_EQ(expected_name, tmp);
}

// Test trace calls made after tracing singleton shut down.
//
// The singleton is destroyed by our base::AtExitManager, but there can be
// code still executing as the C++ static objects are destroyed. This test
// forces the singleton to destroy early, and intentinally makes trace calls
// afterwards.
TEST_F(TraceEventTestFixture, AtExit) {
  // Repeat this test a few times. Besides just showing robustness, it also
  // allows us to test that events at shutdown do not appear with valid events
  // recorded after the system is started again.
  for (int i = 0; i < 4; i++) {
    // Scope to contain the then destroy the TraceLog singleton.
    {
      base::ShadowingAtExitManager exit_manager_will_destroy_singletons;

      // Setup TraceLog singleton inside this test's exit manager scope
      // so that it will be destroyed when this scope closes.
      ManualTestSetUp();

      TRACE_EVENT_INSTANT0("all", "not recorded; system not enabled");

      BeginTrace();

      TRACE_EVENT_INSTANT0("all", "is recorded 1; system has been enabled");
      // Trace calls that will cache pointers to categories; they're valid here
      TraceCallsWithCachedCategoryPointersPointers(
          "is recorded 2; system has been enabled");

      EndTraceAndFlush();
    } // scope to destroy singleton
    ASSERT_FALSE(TraceLog::GetInstance());

    // Now that singleton is destroyed, check what trace events were recorded
    const DictionaryValue* item = NULL;
    ListValue& trace_parsed = trace_parsed_;
    EXPECT_FIND_("is recorded 1");
    EXPECT_FIND_("is recorded 2");
    EXPECT_NOT_FIND_("not recorded");

    // Make additional trace event calls on the shutdown system. They should
    // all pass cleanly, but the data not be recorded. We'll verify that next
    // time around the loop (the only way to flush the trace buffers).
    TRACE_EVENT_BEGIN_ETW("not recorded; system shutdown", 0, NULL);
    TRACE_EVENT_END_ETW("not recorded; system shutdown", 0, NULL);
    TRACE_EVENT_INSTANT_ETW("not recorded; system shutdown", 0, NULL);
    TRACE_EVENT0("all", "not recorded; system shutdown");
    TRACE_EVENT_INSTANT0("all", "not recorded; system shutdown");
    TRACE_EVENT_BEGIN0("all", "not recorded; system shutdown");
    TRACE_EVENT_END0("all", "not recorded; system shutdown");

    TRACE_EVENT0("new category 0!", "not recorded; system shutdown");
    TRACE_EVENT_INSTANT0("new category 1!", "not recorded; system shutdown");
    TRACE_EVENT_BEGIN0("new category 2!", "not recorded; system shutdown");
    TRACE_EVENT_END0("new category 3!", "not recorded; system shutdown");

    // Cached categories should be safe to check, and still disable traces
    TraceCallsWithCachedCategoryPointersPointers(
        "not recorded; system shutdown");
  }
}

TEST_F(TraceEventTestFixture, NormallyNoDeepCopy) {
  // Test that the TRACE_EVENT macros do not deep-copy their string. If they
  // do so it may indicate a performance regression, but more-over it would
  // make the DEEP_COPY overloads redundant.
  ManualTestSetUp();

  std::string name_string("event name");

  BeginTrace();
  TRACE_EVENT_INSTANT0("category", name_string.c_str());

  // Modify the string in place (a wholesale reassignment may leave the old
  // string intact on the heap).
  name_string[0] = '@';

  EndTraceAndFlush();

  EXPECT_FALSE(FindTraceEntry(trace_parsed_, "event name"));
  EXPECT_TRUE(FindTraceEntry(trace_parsed_, name_string.c_str()));
}

TEST_F(TraceEventTestFixture, DeepCopy) {
  ManualTestSetUp();

  static const char kOriginalName1[] = "name1";
  static const char kOriginalName2[] = "name2";
  static const char kOriginalName3[] = "name3";
  std::string name1(kOriginalName1);
  std::string name2(kOriginalName2);
  std::string name3(kOriginalName3);
  std::string arg1("arg1");
  std::string arg2("arg2");
  std::string val1("val1");
  std::string val2("val2");

  BeginTrace();
  TRACE_EVENT_COPY_INSTANT0("category", name1.c_str());
  TRACE_EVENT_COPY_BEGIN1("category", name2.c_str(),
                          arg1.c_str(), 5);
  TRACE_EVENT_COPY_END2("category", name3.c_str(),
                        arg1.c_str(), val1,
                        arg2.c_str(), val2);

  // As per NormallyNoDeepCopy, modify the strings in place.
  name1[0] = name2[0] = name3[0] = arg1[0] = arg2[0] = val1[0] = val2[0] = '@';

  EndTraceAndFlush();

  EXPECT_FALSE(FindTraceEntry(trace_parsed_, name1.c_str()));
  EXPECT_FALSE(FindTraceEntry(trace_parsed_, name2.c_str()));
  EXPECT_FALSE(FindTraceEntry(trace_parsed_, name3.c_str()));

  const DictionaryValue* entry1 = FindTraceEntry(trace_parsed_, kOriginalName1);
  const DictionaryValue* entry2 = FindTraceEntry(trace_parsed_, kOriginalName2);
  const DictionaryValue* entry3 = FindTraceEntry(trace_parsed_, kOriginalName3);
  ASSERT_TRUE(entry1);
  ASSERT_TRUE(entry2);
  ASSERT_TRUE(entry3);

  int i;
  EXPECT_FALSE(entry2->GetInteger("args.@rg1", &i));
  EXPECT_TRUE(entry2->GetInteger("args.arg1", &i));
  EXPECT_EQ(5, i);

  std::string s;
  EXPECT_TRUE(entry3->GetString("args.arg1", &s));
  EXPECT_EQ("val1", s);
  EXPECT_TRUE(entry3->GetString("args.arg2", &s));
  EXPECT_EQ("val2", s);
}

// Test that TraceResultBuffer outputs the correct result whether it is added
// in chunks or added all at once.
TEST_F(TraceEventTestFixture, TraceResultBuffer) {
  ManualTestSetUp();

  Clear();

  trace_buffer_.Start();
  trace_buffer_.AddFragment("bla1");
  trace_buffer_.AddFragment("bla2");
  trace_buffer_.AddFragment("bla3,bla4");
  trace_buffer_.Finish();
  EXPECT_STREQ(json_output_.json_output.c_str(), "[bla1,bla2,bla3,bla4]");

  Clear();

  trace_buffer_.Start();
  trace_buffer_.AddFragment("bla1,bla2,bla3,bla4");
  trace_buffer_.Finish();
  EXPECT_STREQ(json_output_.json_output.c_str(), "[bla1,bla2,bla3,bla4]");
}

}  // namespace debug
}  // namespace base
