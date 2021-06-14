// Copyright 2017 The Cobalt Authors. All Rights Reserved.
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

#include "cobalt/browser/memory_tracker/tool/malloc_logger_tool.h"

#include <algorithm>

#include "base/time/time.h"
#include "cobalt/base/c_val.h"
#include "cobalt/browser/memory_tracker/tool/buffered_file_writer.h"
#include "cobalt/browser/memory_tracker/tool/params.h"
#include "cobalt/browser/memory_tracker/tool/util.h"
#include "nb/memory_scope.h"
#include "starboard/atomic.h"
#include "starboard/common/string.h"
#include "starboard/types.h"

namespace cobalt {
namespace browser {
namespace memory_tracker {

namespace {
const int kAllocationRecord = 1;
const int kDeallocationRecord = 0;
const size_t kStartIndex = 5;
const size_t kNumAddressPrints = 2;
const size_t kMaxStackSize = 10;
const size_t kRecordLimit = 1024;
const NbMemoryScopeInfo kEmptyCallstackMemoryScopeInfo = {nullptr, "-", "-",
                                                          0,       "-", true};
}  // namespace

MallocLoggerTool::MallocLoggerTool()
    : start_time_(NowTime()),
      atomic_counter_(0),
      atomic_used_memory_(SbSystemGetUsedCPUMemory()) {
  buffered_file_writer_.reset(new BufferedFileWriter(MemoryLogPath()));
}

MallocLoggerTool::~MallocLoggerTool() {
  // No locks are used for the thread reporter, so when it's set to null
  // we allow one second for any suspended threads to run through and finish
  // their reporting.
  SbMemorySetReporter(NULL);
  SbThreadSleep(kSbTimeSecond);
  buffered_file_writer_.reset(NULL);
}

std::string MallocLoggerTool::tool_name() const {
  return "MemoryTrackerMallocLogger";
}

void MallocLoggerTool::Run(Params* params) {
  // Update malloc stats every second
  params->logger()->Output("MemoryTrackerMallocLogger running...");

  // There are some memory allocations which do not get tracked.
  // Those allocations show up as fragmentation.
  // It has been empirically observed that a majority (98%+) of these
  // untracked allocations happen in the first 20 seconds of Cobalt runtime.
  //
  // Also, there is minimal external fragmentation (< 1 MB) during this initial
  // period.
  //
  // The following piece of code resets atomic_used_memory_ at the 20 second
  // mark, to compensate for the deviation due to untracked memory.
  base::TimeDelta current_sample_interval = base::TimeDelta::FromSeconds(20);
  if (!params->wait_for_finish_signal(current_sample_interval.ToSbTime())) {
    atomic_used_memory_.store(SbSystemGetUsedCPUMemory());
  }

  // Export fragmentation as a CVal on HUD.
  base::CVal<base::cval::SizeInBytes> memory_fragmentation(
      "Memory.CPU.Fragmentation", base::cval::SizeInBytes(0),
      "Memory Fragmentation");

  // Update CVal every 5 seconds
  current_sample_interval = base::TimeDelta::FromSeconds(5);
  int64_t allocated_memory = 0;
  int64_t used_memory = 0;
  while (!params->wait_for_finish_signal(current_sample_interval.ToSbTime())) {
    allocated_memory = SbSystemGetUsedCPUMemory();
    used_memory = atomic_used_memory_.load();
    memory_fragmentation = static_cast<uint64>(
        std::max(allocated_memory - used_memory, static_cast<int64_t>(0)));
  }
}

void MallocLoggerTool::LogRecord(const void* memory_block,
                                 const nb::analytics::AllocationRecord& record,
                                 const nb::analytics::CallStack& callstack,
                                 int type) {
  const int log_counter = atomic_counter_.increment();
  const int64_t used_memory = atomic_used_memory_.load();
  const int64_t allocated_memory = SbSystemGetUsedCPUMemory();
  const int time_since_start_ms = GetTimeSinceStartMs();
  char buff[kRecordLimit] = {0};
  size_t buff_pos = 0;
  void* addresses[kMaxStackSize];

  const NbMemoryScopeInfo* memory_scope;
  if (callstack.empty()) {
    memory_scope = &kEmptyCallstackMemoryScopeInfo;
  } else {
    memory_scope = callstack.back();
  }

  int bytes_written = SbStringFormatF(
      buff, sizeof(buff),
      "%u,%d,%zd,\"%s\",%d,%s,%d,%" PRId64 ",%" PRId64 ",%" PRIXPTR ",\"",
      log_counter, type, record.size, memory_scope->file_name_,
      memory_scope->line_number_, memory_scope->function_name_,
      time_since_start_ms, allocated_memory, used_memory,
      reinterpret_cast<uintptr_t>(memory_block));

  buff_pos += static_cast<size_t>(bytes_written);
  const size_t count = std::max(SbSystemGetStack(addresses, kMaxStackSize), 0);
  const size_t end_index = std::min(count, kStartIndex + kNumAddressPrints);
  // For each of the stack addresses that we care about, concat them to the
  // buffer. This was originally written to do multiple stack addresses but
  // this tends to overflow on some lower platforms so it's possible that
  // this loop only iterates once.
  for (size_t i = kStartIndex; i < end_index; ++i) {
    void* p = addresses[i];
    bytes_written =
        SbStringFormatF(buff + buff_pos, kRecordLimit - buff_pos,
                        ",%" PRIXPTR "", reinterpret_cast<uintptr_t>(p));
    DCHECK_GE(bytes_written, 0);
    buff_pos += static_cast<size_t>(bytes_written);
  }

  // Adds a "\n" at the end.
  bytes_written = starboard::strlcat(buff + buff_pos, "\"\n",
                                     static_cast<int>(kRecordLimit - buff_pos));
  buff_pos += bytes_written;
  buffered_file_writer_->Append(buff, buff_pos);
}

void MallocLoggerTool::OnMemoryAllocation(
    const void* memory_block, const nb::analytics::AllocationRecord& record,
    const nb::analytics::CallStack& callstack) {
  atomic_used_memory_.fetch_add(record.size);
  LogRecord(memory_block, record, callstack, kAllocationRecord);
}

void MallocLoggerTool::OnMemoryDeallocation(
    const void* memory_block, const nb::analytics::AllocationRecord& record,
    const nb::analytics::CallStack& callstack) {
  atomic_used_memory_.fetch_sub(record.size);
  LogRecord(memory_block, record, callstack, kDeallocationRecord);
}

std::string MallocLoggerTool::MemoryLogPath() {
  char file_name_buff[2048] = {};
  SbSystemGetPath(kSbSystemPathDebugOutputDirectory, file_name_buff,
                  arraysize(file_name_buff));
  std::string path(file_name_buff);
  if (!path.empty()) {  // Protect against a dangling "/" at end.
    const int back_idx_signed = static_cast<int>(path.length()) - 1;
    if (back_idx_signed >= 0) {
      const size_t idx = back_idx_signed;
      if (path[idx] == '/') {
        path.erase(idx);
      }
    }
  }

  base::Time time = base::Time::Now();
  base::Time::Exploded exploded;
  time.LocalExplode(&exploded);

  std::stringstream ss;
  ss << "/memory_log_" << exploded.year << "-" << exploded.month << "-"
     << exploded.day_of_month << ":" << exploded.hour << "-" << exploded.minute
     << "-" << exploded.second << ".csv";
  path.append(ss.str());
  return path;
}

base::TimeTicks MallocLoggerTool::NowTime() {
  // NowFromSystemTime() is slower but more accurate. However it might
  // be useful to use the faster but less accurate version if there is
  // a speedup.
  return base::TimeTicks::Now();
}

int MallocLoggerTool::GetTimeSinceStartMs() const {
  base::TimeDelta dt = NowTime() - start_time_;
  return static_cast<int>(dt.InMilliseconds());
}

}  // namespace memory_tracker
}  // namespace browser
}  // namespace cobalt
