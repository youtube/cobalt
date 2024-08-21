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
#include <cmath>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/memory/ptr_util.h"
#include "base/optional.h"
#include "base/process/internal_linux.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/time/time.h"

namespace base {

namespace {

static std::atomic<int> clock_ticks_per_s{0};

ProcessMetricsHelper::ReadCallback GetReadCallback(const FilePath& path) {
  return BindOnce(
      [](const FilePath& path) -> Optional<std::string> {
        std::string contents;
        if (!ReadFileToString(path, &contents)) return nullopt;
        return contents;
      },
      path);
}

double CalculateCPUUsageSeconds(const std::string& utime_string,
                                const std::string& stime_string,
                                int ticks_per_s) {
  DCHECK_NE(ticks_per_s, 0);
  double utime;
  if (!StringToDouble(utime_string, &utime)) return 0.0;
  double stime;
  if (!StringToDouble(stime_string, &stime)) return 0.0;
  return (utime + stime) / static_cast<double>(ticks_per_s);
}

}  // namespace

// static
int ProcessMetricsHelper::GetClockTicksPerS() {
  return clock_ticks_per_s.load();
}

// static
int ProcessMetricsHelper::GetClockTicksPerS(ReadCallback uptime_callback,
                                            ReadCallback stat_callback) {
  double current_uptime = 0.0;
  {
    auto uptime_contents = std::move(uptime_callback).Run();
    if (!uptime_contents) return 0;
    auto parts = SplitString(*uptime_contents, " ", TRIM_WHITESPACE,
                             SPLIT_WANT_NONEMPTY);
    if (parts.size() == 0 || !StringToDouble(parts[0], &current_uptime) ||
        current_uptime == 0.0) {
      return 0;
    }
  }

  auto fields = GetProcStatFields(std::move(stat_callback),
                                  {internal::ProcStatsFields::VM_STARTTIME});
  if (fields.size() != 1) return 0;
  double process_starttime;
  if (!StringToDouble(fields[0], &process_starttime) ||
      process_starttime == 0.0)
    return 0;
  double ticks_per_s = process_starttime / current_uptime;
  int rounded_up = 10 * static_cast<int>(std::ceil(ticks_per_s / 10.0));
  return rounded_up;
}

// static
void ProcessMetricsHelper::PopulateClockTicksPerS() {
  DCHECK_EQ(clock_ticks_per_s.load(), 0);
  clock_ticks_per_s.store(
      GetClockTicksPerS(GetReadCallback(FilePath("/proc/uptime")),
                        GetReadCallback(FilePath("/proc/self/stat"))));
}

// static
TimeDelta ProcessMetricsHelper::GetCumulativeCPUUsage() {
  int ticks_per_s = clock_ticks_per_s.load();
  if (ticks_per_s == 0) return TimeDelta();
  return GetCPUUsage(FilePath("/proc/self"), ticks_per_s);
}

// static
Value ProcessMetricsHelper::GetCumulativeCPUUsagePerThread() {
  int ticks_per_s = clock_ticks_per_s.load();
  if (ticks_per_s == 0) return Value();
  ListValue cpu_per_thread;
  FileEnumerator file_enum(FilePath("/proc/self/task"), /*recursive=*/false,
                           FileEnumerator::DIRECTORIES);
  for (FilePath path = file_enum.Next(); !path.empty();
       path = file_enum.Next()) {
    Fields fields =
        GetProcStatFields(path, {0, internal::ProcStatsFields::VM_COMM,
                                 internal::ProcStatsFields::VM_UTIME,
                                 internal::ProcStatsFields::VM_STIME});
    if (fields.size() != 4) continue;
    int id;
    if (!StringToInt(fields[0], &id)) continue;
    DictionaryValue entry;
    entry.SetKey("id", Value(id));
    entry.SetKey("name", Value(fields[1]));
    entry.SetKey("utime", Value(fields[2]));
    entry.SetKey("stime", Value(fields[3]));
    entry.SetKey("usage_seconds", Value(CalculateCPUUsageSeconds(
                                      fields[2], fields[3], ticks_per_s)));
    cpu_per_thread.GetList().push_back(std::move(entry));
  }
  return std::move(cpu_per_thread);
}

// static
ProcessMetricsHelper::Fields ProcessMetricsHelper::GetProcStatFields(
    ReadCallback read_callback, std::initializer_list<int> indices) {
  auto contents = std::move(read_callback).Run();
  if (!contents) return Fields();

  std::vector<std::string> proc_stats;
  if (!internal::ParseProcStats(*contents, &proc_stats)) return Fields();

  Fields fields;
  for (int index : indices) {
    if (index < 0 || index >= proc_stats.size()) return Fields();
    fields.push_back(std::move(proc_stats[index]));
  }
  return std::move(fields);
}

// static
ProcessMetricsHelper::Fields ProcessMetricsHelper::GetProcStatFields(
    const FilePath& path, std::initializer_list<int> indices) {
  return ProcessMetricsHelper::GetProcStatFields(
      GetReadCallback(path.Append("stat")), indices);
}

// static
TimeDelta ProcessMetricsHelper::GetCPUUsage(ReadCallback read_callback,
                                            int ticks_per_s) {
  auto fields = ProcessMetricsHelper::GetProcStatFields(
      std::move(read_callback), {internal::ProcStatsFields::VM_UTIME,
                                 internal::ProcStatsFields::VM_STIME});
  if (fields.size() != 2) return TimeDelta();
  return TimeDelta::FromSecondsD(
      CalculateCPUUsageSeconds(fields[0], fields[1], ticks_per_s));
}

// static
TimeDelta ProcessMetricsHelper::GetCPUUsage(const FilePath& path,
                                            int ticks_per_s) {
  return ProcessMetricsHelper::GetCPUUsage(GetReadCallback(path.Append("stat")),
                                           ticks_per_s);
}

}  // namespace base
