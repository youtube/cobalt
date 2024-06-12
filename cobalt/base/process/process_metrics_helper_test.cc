// Copyright 2024 The Cobalt Authors. All Rights Reserved.
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

#include "cobalt/base/process/process_metrics_helper.h"

#include <atomic>
#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/containers/contains.h"
#include "base/strings/string_number_conversions.h"
#include "base/threading/thread.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {

namespace {

void BusyWork(std::vector<std::string>* vec) {
  for (int i = 0; i < 100000; ++i) {
    vec->push_back(NumberToString(i));
  }
}

bool WorkUntilUsageIsDetected(Thread* thread) {
  // Attempt to do work on |thread| until CPU usage can be detected.
  for (int i = 0; i < 10; i++) {
    std::vector<std::string> vec;
    thread->task_runner()->PostTask(FROM_HERE, BindOnce(&BusyWork, &vec));
    thread->FlushForTesting();
    auto cpu_per_thread =
        ProcessMetricsHelper::GetCumulativeCPUUsagePerThread();
    auto* list = cpu_per_thread.GetIfList();
    EXPECT_NE(list, nullptr);
    int found_populated = 0;
    for (auto& entry_value : *list) {
      base::Value::Dict* entry = entry_value.GetIfDict();
      EXPECT_NE(entry, nullptr);
      int id = entry->FindInt("id").value();
      if (id == thread->GetThreadId()) {
        int utime;
        if (!StringToInt(*(entry->FindString("utime")), &utime)) return false;
        int stime;
        if (!StringToInt(*(entry->FindString("stime")), &stime)) return false;
        if (utime + stime > 0) {
          return true;
        }
      }
    }
  }
  return false;
}

void CheckThreadUsage(const base::Value::List& list, PlatformThreadId thread_id,
                      const std::string& thread_name) {
  for (auto& entry_value : list) {
    const base::Value::Dict* entry = entry_value.GetIfDict();
    EXPECT_NE(entry, nullptr);
    EXPECT_TRUE(entry->contains("id"));
    EXPECT_TRUE(entry->contains("name"));
    EXPECT_TRUE(entry->contains("utime"));
    EXPECT_TRUE(entry->contains("stime"));
    EXPECT_TRUE(entry->contains("usage_seconds"));
    int id = entry->FindInt("id").value();
    if (id == thread_id) {
      EXPECT_EQ(thread_name, *(entry->FindString("name")));
      EXPECT_GT(entry->FindString("utime")->size(), 0);
      EXPECT_GT(entry->FindString("stime")->size(), 0);
      EXPECT_GT(entry->FindDouble("usage_seconds").value_or(0.0), 0.0);
      return;
    }
  }
  EXPECT_TRUE(false) << "Thread not found in usage list.";
}

}  // namespace

class ProcessMetricsHelperTest : public testing::Test {
 public:
  static void SetUpTestSuite() {
    ProcessMetricsHelper::PopulateClockTicksPerS();
  }

  ProcessMetricsHelper::Fields GetProcStatFields(
      ProcessMetricsHelper::ReadCallback cb,
      std::initializer_list<int> indices) {
    return ProcessMetricsHelper::GetProcStatFields(std::move(cb), indices);
  }

  int GetClockTicksPerS(ProcessMetricsHelper::ReadCallback uptime_callback,
                        ProcessMetricsHelper::ReadCallback stat_callback) {
    return ProcessMetricsHelper::GetClockTicksPerS(std::move(uptime_callback),
                                                   std::move(stat_callback));
  }

  TimeDelta GetCPUUsage(ProcessMetricsHelper::ReadCallback stat_callback,
                        int ticks_per_s) {
    return ProcessMetricsHelper::GetCPUUsage(std::move(stat_callback),
                                             ticks_per_s);
  }
};

ProcessMetricsHelper::ReadCallback GetNulloptCallback() {
  return BindOnce(
      []() -> absl::optional<std::string> { return absl::nullopt; });
}

ProcessMetricsHelper::ReadCallback GetUptimeCallback() {
  return BindOnce([]() -> absl::optional<std::string> {
    // Example of `cat /proc/uptime`. First number is uptime in seconds.
    return "1635667.97 155819443.97";
  });
}

ProcessMetricsHelper::ReadCallback GetStatCallback() {
  return BindOnce([]() -> absl::optional<std::string> {
    // Example of `cat /proc/<pid>/stat`. The format is described in `man proc`
    // or https://man7.org/linux/man-pages/man5/proc.5.html. `utime`, `stime`,
    // and `starttime` are in clock ticks (commonly 100Hz).
    return "3669677 (name with )( ) R 477564 3669677 477564 34817 3669677 "
           "4194304 91 0 0 0 1 3 0 0 20 0 1 0 163348716 9076736 384 "
           "18446744073709551615 94595846197248 94595846217801 140722879734192 "
           "0 0 0 0 0 0 0 0 0 17 52 0 0 0 0 0 94595846237232 94595846238880 "
           "94595860643840 140722879736274 140722879736294 140722879736294 "
           "140722879741931 0";
  });
}

TEST_F(ProcessMetricsHelperTest, GetProcStatFields) {
  auto fields = GetProcStatFields(GetStatCallback(), {0, 1, 2, 21, 51});
  EXPECT_EQ(5, fields.size());
  EXPECT_EQ("3669677", fields[0]);
  EXPECT_EQ("name with )( ", fields[1]);
  EXPECT_EQ("R", fields[2]);
  EXPECT_EQ("163348716", fields[3]);
  EXPECT_EQ("0", fields[4]);

  fields = GetProcStatFields(GetStatCallback(), {0});
  EXPECT_EQ(1, fields.size());
  EXPECT_EQ("3669677", fields[0]);

  fields = GetProcStatFields(GetStatCallback(), {0, 52});
  EXPECT_EQ(0, fields.size());

  fields = GetProcStatFields(GetNulloptCallback(), {0});
  EXPECT_EQ(0, fields.size());
}

TEST_F(ProcessMetricsHelperTest, GetClockTicksPerSWithCallbacks) {
  EXPECT_EQ(0, GetClockTicksPerS(GetNulloptCallback(), GetNulloptCallback()));
  EXPECT_EQ(0, GetClockTicksPerS(GetNulloptCallback(), GetStatCallback()));
  EXPECT_EQ(0, GetClockTicksPerS(GetUptimeCallback(), GetNulloptCallback()));
  EXPECT_EQ(100, GetClockTicksPerS(GetUptimeCallback(), GetStatCallback()));
}

TEST_F(ProcessMetricsHelperTest, GetClockTicksPerS) {
  EXPECT_EQ(100, ProcessMetricsHelper::GetClockTicksPerS());
}

TEST_F(ProcessMetricsHelperTest, GetCumulativeCPUUsage) {
  TimeDelta usage = ProcessMetricsHelper::GetCumulativeCPUUsage();
  EXPECT_GE(usage.InMicroseconds(), 0);
}

TEST_F(ProcessMetricsHelperTest, GetCPUUsage) {
  TimeDelta usage = GetCPUUsage(GetStatCallback(), 100);
  EXPECT_EQ(40, usage.InMilliseconds());
}

TEST_F(ProcessMetricsHelperTest, GetCumulativeCPUUsagePerThread) {
  int initial_num_threads;
  {
    base::Value cpu_per_thread =
        ProcessMetricsHelper::GetCumulativeCPUUsagePerThread();
    base::Value::List* list = cpu_per_thread.GetIfList();
    initial_num_threads = list ? list->size() : 0;
  }

  Thread thread1("thread1");
  Thread thread2("thread2");
  Thread thread3("thread3");

  thread1.StartAndWaitForTesting();
  thread2.StartAndWaitForTesting();
  thread3.StartAndWaitForTesting();

  ASSERT_TRUE(thread1.IsRunning());
  ASSERT_TRUE(thread2.IsRunning());
  ASSERT_TRUE(thread3.IsRunning());

  EXPECT_TRUE(WorkUntilUsageIsDetected(&thread1));
  EXPECT_TRUE(WorkUntilUsageIsDetected(&thread2));
  EXPECT_TRUE(WorkUntilUsageIsDetected(&thread3));

  auto cpu_per_thread = ProcessMetricsHelper::GetCumulativeCPUUsagePerThread();
  auto* list = cpu_per_thread.GetIfList();
  EXPECT_NE(list, nullptr);
  EXPECT_EQ(initial_num_threads + 3, list->size());
  CheckThreadUsage(*list, thread1.GetThreadId(), thread1.thread_name());
  CheckThreadUsage(*list, thread2.GetThreadId(), thread2.thread_name());
  CheckThreadUsage(*list, thread3.GetThreadId(), thread3.thread_name());
  thread1.Stop();
  thread2.Stop();
  thread3.Stop();
}

}  // namespace base
