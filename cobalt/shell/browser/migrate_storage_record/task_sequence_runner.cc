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

#include "cobalt/shell/browser/migrate_storage_record/task_sequence_runner.h"

#include "base/functional/bind.h"

namespace cobalt {
namespace migrate_storage_record {

TaskSequenceRunner::TaskSequenceRunner(base::OnceClosure done_callback)
    : final_callback_(std::move(done_callback)) {}

TaskSequenceRunner::~TaskSequenceRunner() = default;

void TaskSequenceRunner::AddTask(Task task) {
  tasks_.push(std::move(task));
}

void TaskSequenceRunner::RunNext() {
  if (tasks_.empty()) {
    if (final_callback_) {
      std::move(final_callback_).Run();
    }
    // Delete this instance as it has finished its lifecycle.
    delete this;
    return;
  }

  Task next = std::move(tasks_.front());
  tasks_.pop();

  // Bind RunNext to the completion of the current task.
  // We use base::Unretained(this) because the class instance is responsible
  // for its own deletion only after the queue is empty.
  std::move(next).Run(
      base::BindOnce(&TaskSequenceRunner::RunNext, base::Unretained(this)));
}

}  // namespace migrate_storage_record
}  // namespace cobalt
