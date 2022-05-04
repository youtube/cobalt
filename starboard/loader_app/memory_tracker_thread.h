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

#ifndef STARBOARD_LOADER_APP_MEMORY_TRACKER_THREAD_H_
#define STARBOARD_LOADER_APP_MEMORY_TRACKER_THREAD_H_

#include "starboard/common/thread.h"
#include "starboard/types.h"

namespace starboard {
namespace loader_app {

// Periodically queries for and logs process memory usage when enabled.
class MemoryTrackerThread : public starboard::Thread {
 public:
  explicit MemoryTrackerThread(int period_in_millis = 100);
  ~MemoryTrackerThread();

 private:
  void Run() override;

  const int period_in_millis_;
  int64_t max_used_cpu_memory_ = 0;
};

}  // namespace loader_app
}  // namespace starboard

#endif  // STARBOARD_LOADER_APP_MEMORY_TRACKER_THREAD_H_
