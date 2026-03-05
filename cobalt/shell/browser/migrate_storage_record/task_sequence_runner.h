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

#ifndef COBALT_SHELL_BROWSER_MIGRATE_STORAGE_RECORD_TASK_SEQUENCE_RUNNER_H_
#define COBALT_SHELL_BROWSER_MIGRATE_STORAGE_RECORD_TASK_SEQUENCE_RUNNER_H_

#include <queue>
#include <utility>

#include "base/functional/callback.h"

namespace cobalt {
namespace migrate_storage_record {

// Used to define a task that accepts a completion closure.
using Task = base::OnceCallback<void(base::OnceClosure)>;

// Helper class to run a sequence of move-only tasks safely.
// This class is self-managed and deletes itself once the task queue is empty
// and the final callback has been invoked.
class TaskSequenceRunner {
 public:
  explicit TaskSequenceRunner(base::OnceClosure done_callback);

  TaskSequenceRunner(const TaskSequenceRunner&) = delete;
  TaskSequenceRunner& operator=(const TaskSequenceRunner&) = delete;

  ~TaskSequenceRunner();

  // Adds a task to the back of the execution queue.
  void AddTask(Task task);

  // Begins or continues execution of the queued tasks.
  void RunNext();

 private:
  std::queue<Task> tasks_;
  base::OnceClosure final_callback_;
};

}  // namespace migrate_storage_record
}  // namespace cobalt

#endif  // COBALT_SHELL_BROWSER_MIGRATE_STORAGE_RECORD_TASK_SEQUENCE_RUNNER_H_
