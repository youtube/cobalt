// Copyright 2026 The Cobalt Authors. All Rights Reserved.
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

#ifndef COBALT_MEMORY_COBALT_MEMORY_TASK_OBSERVER_H_
#define COBALT_MEMORY_COBALT_MEMORY_TASK_OBSERVER_H_

#include <memory>
#include <vector>

#include "base/memory/cobalt_memory_context.h"
#include "base/task/task_observer.h"

namespace cobalt {
namespace memory {

class CobaltMemoryTaskObserver : public base::TaskObserver {
 public:
  CobaltMemoryTaskObserver();
  ~CobaltMemoryTaskObserver() override;

  // base::TaskObserver implementation
  void WillProcessTask(const base::PendingTask& pending_task,
                       bool was_blocked_or_low_priority) override;
  void DidProcessTask(const base::PendingTask& pending_task) override;

 private:
  base::memory::MemoryContext MapFileToContext(const char* file_name);
};

}  // namespace memory
}  // namespace cobalt

#endif  // COBALT_MEMORY_COBALT_MEMORY_TASK_OBSERVER_H_
