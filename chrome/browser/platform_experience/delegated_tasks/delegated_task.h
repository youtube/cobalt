// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PLATFORM_EXPERIENCE_DELEGATED_TASKS_DELEGATED_TASK_H_
#define CHROME_BROWSER_PLATFORM_EXPERIENCE_DELEGATED_TASKS_DELEGATED_TASK_H_

#include "base/time/time.h"

namespace platform_experience {

// Outcome status of executing a delegated task.
enum class DelegatedTaskStatus {
  kSuccess = 0,
  kFailure = 1,
  kMaxValue = kFailure,
};

enum class DelegatedTaskType {
  kRegisterSearchPromotion = 0,
  kMaxValue = kRegisterSearchPromotion,
};

// Interface/Base class defining a delegated logic task. Concrete
// implementations define the task type, timeout, and exit code interpretation.
class DelegatedTask {
 public:
  virtual ~DelegatedTask() = default;

  // The delegated task type to be executed.
  virtual DelegatedTaskType GetTaskType() const = 0;

  // The execution timeout for this task. Defaults to 10 seconds.
  virtual base::TimeDelta GetTimeout() const;

  // Maps task-specific exit codes that are not handled by the
  // `PehDelegatedTaskRunner` to a ResultStatus.
  virtual DelegatedTaskStatus ParseExitCode(int exit_code) const = 0;
};

}  // namespace platform_experience

#endif  // CHROME_BROWSER_PLATFORM_EXPERIENCE_DELEGATED_TASKS_DELEGATED_TASK_H_
