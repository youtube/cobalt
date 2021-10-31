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

#include "cobalt/browser/memory_tracker/tool/log_writer_tool.h"

#include <algorithm>

#include "base/time/time.h"
#include "cobalt/browser/memory_tracker/tool/buffered_file_writer.h"
#include "cobalt/browser/memory_tracker/tool/params.h"
#include "starboard/client_porting/poem/string_poem.h"
#include "starboard/common/string.h"
#include "starboard/configuration.h"
#include "starboard/configuration_constants.h"
#include "starboard/types.h"

namespace cobalt {
namespace browser {
namespace memory_tracker {

LogWriterTool::LogWriterTool() : start_time_(NowTime()) {
  buffered_file_writer_.reset(new BufferedFileWriter(MemoryLogPath()));
  InitAndRegisterMemoryReporter();
}

LogWriterTool::~LogWriterTool() {
  // No locks are used for the thread reporter, so when it's set to null
  // we allow one second for any suspended threads to run through and finish
  // their reporting.
  SbMemorySetReporter(NULL);
  SbThreadSleep(kSbTimeSecond);
  buffered_file_writer_.reset(NULL);
}

std::string LogWriterTool::tool_name() const {
  return "MemoryTrackerLogWriter";
}

void LogWriterTool::Run(Params* params) {
  // Run function does almost nothing.
  params->logger()->Output("MemoryTrackerLogWriter running...");
}

void LogWriterTool::OnMemoryAllocation(const void* memory_block, size_t size) {
  void* addresses[kMaxStackSize] = {};
  // Though the SbSystemGetStack API documentation does not specify any possible
  // negative return values, we take no chance.
  const size_t count = std::max(SbSystemGetStack(addresses, kMaxStackSize), 0);

  const size_t n = 256;
  char buff[n] = {0};
  size_t buff_pos = 0;

  int time_since_start_ms = GetTimeSinceStartMs();
  // Writes "+ <ALLOCATION ADDRESS> <size> <time>"
  int bytes_written =
      SbStringFormatF(buff, sizeof(buff), "+ %" PRIXPTR " %x %d",
                      reinterpret_cast<uintptr_t>(memory_block),
                      static_cast<unsigned int>(size), time_since_start_ms);

  buff_pos += bytes_written;
  const size_t end_index = std::min(count, kStartIndex + kNumAddressPrints);

  // For each of the stack addresses that we care about, concat them to the
  // buffer. This was originally written to do multiple stack addresses but
  // this tends to overflow on some lower platforms so it's possible that
  // this loop only iterates once.
  for (size_t i = kStartIndex; i < end_index; ++i) {
    void* p = addresses[i];
    bytes_written =
        SbStringFormatF(buff + buff_pos, sizeof(buff) - buff_pos,
                        " %" PRIXPTR "", reinterpret_cast<uintptr_t>(p));
    DCHECK_GE(bytes_written, 0);

    if (bytes_written < 0) {
      DCHECK(false) << "Error occurred while writing string.";
      continue;
    }

    buff_pos += static_cast<size_t>(bytes_written);
  }
  // Adds a "\n" at the end.
  starboard::strlcat(buff + buff_pos, "\n", static_cast<int>(n - buff_pos));
  buffered_file_writer_->Append(buff, strlen(buff));
}

void LogWriterTool::OnMemoryDeallocation(const void* memory_block) {
  const size_t n = 256;
  char buff[n] = {0};
  // Writes "- <ADDRESS OF ALLOCATION> \n"
  SbStringFormatF(buff, sizeof(buff), "- %" PRIXPTR "\n",
                  reinterpret_cast<uintptr_t>(memory_block));
  buffered_file_writer_->Append(buff, strlen(buff));
}

void LogWriterTool::OnAlloc(void* context, const void* memory, size_t size) {
  LogWriterTool* self = static_cast<LogWriterTool*>(context);
  self->OnMemoryAllocation(memory, size);
}

void LogWriterTool::OnDealloc(void* context, const void* memory) {
  LogWriterTool* self = static_cast<LogWriterTool*>(context);
  self->OnMemoryDeallocation(memory);
}

void LogWriterTool::OnMapMemory(void* context, const void* memory,
                                size_t size) {
  LogWriterTool* self = static_cast<LogWriterTool*>(context);
  self->OnMemoryAllocation(memory, size);
}

void LogWriterTool::OnUnMapMemory(void* context, const void* memory,
                                  size_t size) {
  LogWriterTool* self = static_cast<LogWriterTool*>(context);
  self->OnMemoryDeallocation(memory);
}

std::string LogWriterTool::MemoryLogPath() {
  char file_name_buff[2048] = {};
  SbSystemGetPath(kSbSystemPathDebugOutputDirectory, file_name_buff,
                  arraysize(file_name_buff));
  std::string path(file_name_buff);
  if (!path.empty()) {  // Protect against a dangling "/" at end.
    const int back_idx_signed = static_cast<int>(path.length()) - 1;
    if (back_idx_signed >= 0) {
      const size_t idx = back_idx_signed;
      if (path[idx] == kSbFileSepChar) {
        path.erase(idx);
      }
    }
  }
  path.push_back(kSbFileSepChar);
  path.append("memory_log.txt");
  return path;
}

base::TimeTicks LogWriterTool::NowTime() {
  // NowFromSystemTime() is slower but more accurate. However it might
  // be useful to use the faster but less accurate version if there is
  // a speedup.
  return base::TimeTicks::Now();
}

int LogWriterTool::GetTimeSinceStartMs() const {
  base::TimeDelta dt = NowTime() - start_time_;
  return static_cast<int>(dt.InMilliseconds());
}

void LogWriterTool::InitAndRegisterMemoryReporter() {
  DCHECK(!memory_reporter_.get()) << "Memory Reporter already registered.";

  SbMemoryReporter mem_reporter = {OnAlloc, OnDealloc, OnMapMemory,
                                   OnUnMapMemory, this};
  memory_reporter_.reset(new SbMemoryReporter(mem_reporter));
  SbMemorySetReporter(memory_reporter_.get());
}

}  // namespace memory_tracker
}  // namespace browser
}  // namespace cobalt
