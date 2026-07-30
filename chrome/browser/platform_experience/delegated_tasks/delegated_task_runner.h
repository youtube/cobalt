// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PLATFORM_EXPERIENCE_DELEGATED_TASKS_DELEGATED_TASK_RUNNER_H_
#define CHROME_BROWSER_PLATFORM_EXPERIENCE_DELEGATED_TASKS_DELEGATED_TASK_RUNNER_H_

#include "base/functional/callback_forward.h"
#include "base/time/time.h"
#include "chrome/browser/platform_experience/delegated_tasks/delegated_task.h"

namespace platform_experience {

struct DelegatedTaskResult {
  DelegatedTaskStatus status;
  base::TimeDelta execution_time;
};

// `DelegatedTaskRunner` handles PEH binary path resolution, binary
// verification, executing tasks, waiting for task completion/timeout and UMA
// telemetry.
class DelegatedTaskRunner {
 public:
  DelegatedTaskRunner() = default;
  ~DelegatedTaskRunner() = default;

  DelegatedTaskRunner(const DelegatedTaskRunner&) = delete;
  DelegatedTaskRunner& operator=(const DelegatedTaskRunner&) = delete;

  void Run(const DelegatedTask& task,
           base::OnceCallback<void(DelegatedTaskResult)> callback);
};

}  // namespace platform_experience

#endif  // CHROME_BROWSER_PLATFORM_EXPERIENCE_DELEGATED_TASKS_DELEGATED_TASK_RUNNER_H_
