// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Test of classes in the tracked_objects.h classes.

#include "base/tracked_objects.h"

#include "base/json/json_writer.h"
#include "base/memory/scoped_ptr.h"
#include "base/time.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace tracked_objects {

class TrackedObjectsTest : public testing::Test {
 public:
   ~TrackedObjectsTest() {
     ThreadData::ShutdownSingleThreadedCleanup();
   }

};

TEST_F(TrackedObjectsTest, MinimalStartupShutdown) {
  // Minimal test doesn't even create any tasks.
  if (!ThreadData::StartTracking(true))
    return;

  EXPECT_FALSE(ThreadData::first());  // No activity even on this thread.
  ThreadData* data = ThreadData::Get();
  EXPECT_TRUE(ThreadData::first());  // Now class was constructed.
  EXPECT_TRUE(data);
  EXPECT_TRUE(!data->next());
  EXPECT_EQ(data, ThreadData::Get());
  ThreadData::BirthMap birth_map;
  data->SnapshotBirthMap(&birth_map);
  EXPECT_EQ(0u, birth_map.size());
  ThreadData::DeathMap death_map;
  data->SnapshotDeathMap(&death_map);
  EXPECT_EQ(0u, death_map.size());
  ThreadData::ShutdownSingleThreadedCleanup();

  // Do it again, just to be sure we reset state completely.
  ThreadData::StartTracking(true);
  EXPECT_FALSE(ThreadData::first());  // No activity even on this thread.
  data = ThreadData::Get();
  EXPECT_TRUE(ThreadData::first());  // Now class was constructed.
  EXPECT_TRUE(data);
  EXPECT_TRUE(!data->next());
  EXPECT_EQ(data, ThreadData::Get());
  birth_map.clear();
  data->SnapshotBirthMap(&birth_map);
  EXPECT_EQ(0u, birth_map.size());
  death_map.clear();
  data->SnapshotDeathMap(&death_map);
  EXPECT_EQ(0u, death_map.size());
}

TEST_F(TrackedObjectsTest, TinyStartupShutdown) {
  if (!ThreadData::StartTracking(true))
    return;

  // Instigate tracking on a single tracked object, on our thread.
  const Location& location = FROM_HERE;
  ThreadData::TallyABirthIfActive(location);

  const ThreadData* data = ThreadData::first();
  ASSERT_TRUE(data);
  EXPECT_TRUE(!data->next());
  EXPECT_EQ(data, ThreadData::Get());
  ThreadData::BirthMap birth_map;
  data->SnapshotBirthMap(&birth_map);
  EXPECT_EQ(1u, birth_map.size());                         // 1 birth location.
  EXPECT_EQ(1, birth_map.begin()->second->birth_count());  // 1 birth.
  ThreadData::DeathMap death_map;
  data->SnapshotDeathMap(&death_map);
  EXPECT_EQ(0u, death_map.size());                         // No deaths.


  // Now instigate a birth, and a death.
  const Births* second_birth = ThreadData::TallyABirthIfActive(location);
  ThreadData::TallyADeathIfActive(
      second_birth,
      base::TimeTicks(), /* Bogus post_time. */
      base::TimeTicks(), /* Bogus delayed_start_time. */
      base::TimeTicks(), /* Bogus start_run_time. */
      base::TimeTicks()  /* Bogus end_run_time */ );

  birth_map.clear();
  data->SnapshotBirthMap(&birth_map);
  EXPECT_EQ(1u, birth_map.size());                         // 1 birth location.
  EXPECT_EQ(2, birth_map.begin()->second->birth_count());  // 2 births.
  death_map.clear();
  data->SnapshotDeathMap(&death_map);
  EXPECT_EQ(1u, death_map.size());                         // 1 location.
  EXPECT_EQ(1, death_map.begin()->second.count());         // 1 death.

  // The births were at the same location as the one known death.
  EXPECT_EQ(birth_map.begin()->second, death_map.begin()->first);
}

TEST_F(TrackedObjectsTest, DeathDataTest) {
  if (!ThreadData::StartTracking(true))
    return;

  scoped_ptr<DeathData> data(new DeathData());
  ASSERT_NE(data, reinterpret_cast<DeathData*>(NULL));
  EXPECT_EQ(data->run_duration(), base::TimeDelta());
  EXPECT_EQ(data->queue_duration(), base::TimeDelta());
  EXPECT_EQ(data->AverageMsRunDuration(), 0);
  EXPECT_EQ(data->AverageMsQueueDuration(), 0);
  EXPECT_EQ(data->count(), 0);

  int run_ms = 42;
  int queue_ms = 8;

  base::TimeDelta run_duration = base::TimeDelta().FromMilliseconds(run_ms);
  base::TimeDelta queue_duration = base::TimeDelta().FromMilliseconds(queue_ms);
  data->RecordDeath(queue_duration, run_duration);
  EXPECT_EQ(data->run_duration(), run_duration);
  EXPECT_EQ(data->queue_duration(), queue_duration);
  EXPECT_EQ(data->AverageMsRunDuration(), run_ms);
  EXPECT_EQ(data->AverageMsQueueDuration(), queue_ms);
  EXPECT_EQ(data->count(), 1);

  data->RecordDeath(queue_duration, run_duration);
  EXPECT_EQ(data->run_duration(), run_duration + run_duration);
  EXPECT_EQ(data->queue_duration(), queue_duration + queue_duration);
  EXPECT_EQ(data->AverageMsRunDuration(), run_ms);
  EXPECT_EQ(data->AverageMsQueueDuration(), queue_ms);
  EXPECT_EQ(data->count(), 2);

  scoped_ptr<base::DictionaryValue> dictionary(data->ToValue());
  int integer;
  EXPECT_TRUE(dictionary->GetInteger("run_ms", &integer));
  EXPECT_EQ(integer, 2 * run_ms);
  EXPECT_TRUE(dictionary->GetInteger("queue_ms", &integer));
  EXPECT_EQ(integer, 2* queue_ms);
  EXPECT_TRUE(dictionary->GetInteger("count", &integer));
  EXPECT_EQ(integer, 2);

  std::string output;
  data->WriteHTML(&output);
  std::string results = "Lives:2, Run:84ms(42ms/life) Queue:16ms(8ms/life) ";
  EXPECT_EQ(output, results);

  scoped_ptr<base::Value> value(data->ToValue());
  std::string json;
  base::JSONWriter::Write(value.get(), false, &json);
  std::string birth_only_result = "{\"count\":2,\"queue_ms\":16,\"run_ms\":84}";
  EXPECT_EQ(json, birth_only_result);
}

TEST_F(TrackedObjectsTest, BirthOnlyToValueWorkerThread) {
  if (!ThreadData::StartTracking(true))
    return;
  // We don't initialize system with a thread name, so we're viewed as a worker
  // thread.
  int fake_line_number = 173;
  const char* kFile = "FixedFileName";
  const char* kFunction = "BirthOnlyToValueWorkerThread";
  Location location(kFunction, kFile, fake_line_number, NULL);
  Births* birth = ThreadData::TallyABirthIfActive(location);
  EXPECT_NE(birth, reinterpret_cast<Births*>(NULL));

  int process_type = 3;
  scoped_ptr<base::Value> value(ThreadData::ToValue(process_type));
  std::string json;
  base::JSONWriter::Write(value.get(), false, &json);
  std::string birth_only_result = "{"
      "\"list\":["
        "{"
          "\"birth_thread\":\"WorkerThread-1\","
          "\"death_data\":{"
            "\"count\":1,"
            "\"queue_ms\":0,"
            "\"run_ms\":0"
          "},"
          "\"death_thread\":\"Still_Alive\","
          "\"location\":{"
            "\"file_name\":\"FixedFileName\","
            "\"function_name\":\"BirthOnlyToValueWorkerThread\","
            "\"line_number\":173"
          "}"
        "}"
      "],"
      "\"process\":3"
    "}";
  EXPECT_EQ(json, birth_only_result);
}

TEST_F(TrackedObjectsTest, BirthOnlyToValueMainThread) {
  if (!ThreadData::StartTracking(true))
    return;

  // Use a well named thread.
  ThreadData::InitializeThreadContext("SomeMainThreadName");
  int fake_line_number = 173;
  const char* kFile = "FixedFileName";
  const char* kFunction = "BirthOnlyToValueMainThread";
  Location location(kFunction, kFile, fake_line_number, NULL);
  // Do not delete birth.  We don't own it.
  Births* birth = ThreadData::TallyABirthIfActive(location);
  EXPECT_NE(birth, reinterpret_cast<Births*>(NULL));

  int process_type = 34;
  scoped_ptr<base::Value> value(ThreadData::ToValue(process_type));
  std::string json;
  base::JSONWriter::Write(value.get(), false, &json);
  std::string birth_only_result = "{"
      "\"list\":["
        "{"
          "\"birth_thread\":\"SomeMainThreadName\","
          "\"death_data\":{"
            "\"count\":1,"
            "\"queue_ms\":0,"
            "\"run_ms\":0"
          "},"
          "\"death_thread\":\"Still_Alive\","
          "\"location\":{"
            "\"file_name\":\"FixedFileName\","
            "\"function_name\":\"BirthOnlyToValueMainThread\","
            "\"line_number\":173"
          "}"
        "}"
      "],"
      "\"process\":34"
    "}";
  EXPECT_EQ(json, birth_only_result);
}

TEST_F(TrackedObjectsTest, LifeCycleToValueMainThread) {
  if (!ThreadData::StartTracking(true))
    return;

  // Use a well named thread.
  ThreadData::InitializeThreadContext("SomeMainThreadName");
  int fake_line_number = 236;
  const char* kFile = "FixedFileName";
  const char* kFunction = "LifeCycleToValueMainThread";
  Location location(kFunction, kFile, fake_line_number, NULL);
  // Do not delete birth.  We don't own it.
  Births* birth = ThreadData::TallyABirthIfActive(location);
  EXPECT_NE(birth, reinterpret_cast<Births*>(NULL));

  // TimeTicks initializers ar ein microseconds.  Durations are calculated in
  // milliseconds, so we need to use 1000x.
  const base::TimeTicks time_posted = base::TimeTicks() +
      base::TimeDelta::FromMilliseconds(1);
  const base::TimeTicks delayed_start_time = base::TimeTicks();
  const base::TimeTicks start_of_run = base::TimeTicks() +
      base::TimeDelta::FromMilliseconds(5);
  const base::TimeTicks end_of_run = base::TimeTicks() +
      base::TimeDelta::FromMilliseconds(7);
  ThreadData::TallyADeathIfActive(birth, time_posted, delayed_start_time,
                                  start_of_run, end_of_run);

  int process_type = 7;
  scoped_ptr<base::Value> value(ThreadData::ToValue(process_type));
  std::string json;
  base::JSONWriter::Write(value.get(), false, &json);
  std::string one_line_result = "{"
      "\"list\":["
        "{"
          "\"birth_thread\":\"SomeMainThreadName\","
          "\"death_data\":{"
            "\"count\":1,"
            "\"queue_ms\":4,"
            "\"run_ms\":2"
          "},"
          "\"death_thread\":\"SomeMainThreadName\","
          "\"location\":{"
            "\"file_name\":\"FixedFileName\","
            "\"function_name\":\"LifeCycleToValueMainThread\","
            "\"line_number\":236"
          "}"
        "}"
      "],"
      "\"process\":7"
    "}";
  EXPECT_EQ(json, one_line_result);
}

TEST_F(TrackedObjectsTest, TwoLives) {
  if (!ThreadData::StartTracking(true))
    return;

  // Use a well named thread.
  ThreadData::InitializeThreadContext("SomeFileThreadName");
  int fake_line_number = 222;
  const char* kFile = "AnotherFileName";
  const char* kFunction = "TwoLives";
  Location location(kFunction, kFile, fake_line_number, NULL);
  // Do not delete birth.  We don't own it.
  Births* birth = ThreadData::TallyABirthIfActive(location);
  EXPECT_NE(birth, reinterpret_cast<Births*>(NULL));

  // TimeTicks initializers ar ein microseconds.  Durations are calculated in
  // milliseconds, so we need to use 1000x.
  const base::TimeTicks time_posted = base::TimeTicks() +
      base::TimeDelta::FromMilliseconds(1);
  const base::TimeTicks delayed_start_time = base::TimeTicks();
  const base::TimeTicks start_of_run = base::TimeTicks() +
      base::TimeDelta::FromMilliseconds(5);
  const base::TimeTicks end_of_run = base::TimeTicks() +
      base::TimeDelta::FromMilliseconds(7);
  ThreadData::TallyADeathIfActive(birth, time_posted, delayed_start_time,
                                  start_of_run, end_of_run);

  birth = ThreadData::TallyABirthIfActive(location);
  ThreadData::TallyADeathIfActive(birth, time_posted, delayed_start_time,
                                  start_of_run, end_of_run);

  int process_type = 7;
  scoped_ptr<base::Value> value(ThreadData::ToValue(process_type));
  std::string json;
  base::JSONWriter::Write(value.get(), false, &json);
  std::string one_line_result = "{"
      "\"list\":["
        "{"
          "\"birth_thread\":\"SomeFileThreadName\","
          "\"death_data\":{"
            "\"count\":2,"
            "\"queue_ms\":8,"
            "\"run_ms\":4"
          "},"
          "\"death_thread\":\"SomeFileThreadName\","
          "\"location\":{"
            "\"file_name\":\"AnotherFileName\","
            "\"function_name\":\"TwoLives\","
            "\"line_number\":222"
          "}"
        "}"
      "],"
      "\"process\":7"
    "}";
  EXPECT_EQ(json, one_line_result);
}

TEST_F(TrackedObjectsTest, DifferentLives) {
  if (!ThreadData::StartTracking(true))
    return;

  // Use a well named thread.
  ThreadData::InitializeThreadContext("SomeFileThreadName");
  int fake_line_number = 567;
  const char* kFile = "AnotherFileName";
  const char* kFunction = "DifferentLives";
  Location location(kFunction, kFile, fake_line_number, NULL);
  // Do not delete birth.  We don't own it.
  Births* birth = ThreadData::TallyABirthIfActive(location);
  EXPECT_NE(birth, reinterpret_cast<Births*>(NULL));

  // TimeTicks initializers ar ein microseconds.  Durations are calculated in
  // milliseconds, so we need to use 1000x.
  const base::TimeTicks time_posted = base::TimeTicks() +
      base::TimeDelta::FromMilliseconds(1);
  const base::TimeTicks delayed_start_time = base::TimeTicks();
  const base::TimeTicks start_of_run = base::TimeTicks() +
      base::TimeDelta::FromMilliseconds(5);
  const base::TimeTicks end_of_run = base::TimeTicks() +
      base::TimeDelta::FromMilliseconds(7);
  ThreadData::TallyADeathIfActive(birth, time_posted, delayed_start_time,
                                  start_of_run, end_of_run);

  int second_fake_line_number = 999;
  Location second_location(kFunction, kFile, second_fake_line_number, NULL);
  birth = ThreadData::TallyABirthIfActive(second_location);

  int process_type = 2;
  scoped_ptr<base::Value> value(ThreadData::ToValue(process_type));
  std::string json;
  base::JSONWriter::Write(value.get(), false, &json);
  std::string one_line_result = "{"
      "\"list\":["
        "{"
          "\"birth_thread\":\"SomeFileThreadName\","
          "\"death_data\":{"
            "\"count\":1,"
            "\"queue_ms\":4,"
            "\"run_ms\":2"
          "},"
          "\"death_thread\":\"SomeFileThreadName\","
          "\"location\":{"
            "\"file_name\":\"AnotherFileName\","
            "\"function_name\":\"DifferentLives\","
            "\"line_number\":567"
          "}"
        "},"
        "{"
          "\"birth_thread\":\"SomeFileThreadName\","
          "\"death_data\":{"
            "\"count\":1,"
            "\"queue_ms\":0,"
            "\"run_ms\":0"
          "},"
          "\"death_thread\":\"Still_Alive\","
          "\"location\":{"
            "\"file_name\":\"AnotherFileName\","
            "\"function_name\":\"DifferentLives\","
            "\"line_number\":999"
          "}"
        "}"
      "],"
    "\"process\":2"
    "}";
  EXPECT_EQ(json, one_line_result);
}

}  // namespace tracked_objects
