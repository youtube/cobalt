// Copyright 2022 The Cobalt Authors. All Rights Reserved.
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

#include "starboard/loader_app/memory_tracker_thread.h"

namespace starboard {
namespace loader_app {

MemoryTrackerThread::MemoryTrackerThread(int period_in_millis)
    : Thread("MemoryTrackerTh"), period_in_millis_(period_in_millis) {}

MemoryTrackerThread::~MemoryTrackerThread() {
  SB_LOG(INFO) << "Used max of " << max_used_cpu_memory_
               << " bytes of CPU memory";
  Join();
}

void MemoryTrackerThread::Run() {
  int64_t used_cpu_memory = 0;
  while (!join_called()) {
    used_cpu_memory = SbSystemGetUsedCPUMemory();
    SB_LOG(INFO) << "Using " << used_cpu_memory << " of "
                 << SbSystemGetTotalCPUMemory() << " bytes of available "
                 << "CPU Memory";
    if (used_cpu_memory > max_used_cpu_memory_) {
      max_used_cpu_memory_ = used_cpu_memory;
    }
    SleepMilliseconds(period_in_millis_);
  }
}

}  // namespace loader_app
}  // namespace starboard
