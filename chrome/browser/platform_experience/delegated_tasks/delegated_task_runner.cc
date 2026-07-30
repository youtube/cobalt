// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/platform_experience/delegated_tasks/delegated_task_runner.h"

#include <memory>
#include <utility>

#include "base/check.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/location.h"
#include "base/process/launch.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "chrome/browser/platform_experience/delegated_tasks/peh_launcher.h"
#include "chrome/browser/platform_experience/delegated_tasks/peh_switches.h"

namespace platform_experience {

namespace {

// Callbacks must never be dependant on the `DelegatedTaskRunner` instance as
// they can run after the runner instance is destroyed.
void ReturnTaskCompletionStatusAsync(DelegatedTaskStatus status,
                                     base::TimeDelta execution_time,
                                     DelegatedTaskCompletionCallback callback) {
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback),
                                DelegatedTaskResult{status, execution_time}));
}

}  // namespace

DelegatedTaskRunner::DelegatedTaskRunner()
    : DelegatedTaskRunner(std::make_unique<PehLauncher>()) {}

DelegatedTaskRunner::DelegatedTaskRunner(
    std::unique_ptr<PehLauncher> peh_launcher)
    : peh_launcher_(std::move(peh_launcher)) {
  CHECK(peh_launcher_);
}

DelegatedTaskRunner::~DelegatedTaskRunner() {
  if (completion_callback_) {
    CleanupAndReturnResult(
        DelegatedTaskStatus::kRunnerDestroyedBeforeTaskCompletion);
  }
}

void DelegatedTaskRunner::Run(
    const DelegatedTask& task,
    base::OnceCallback<void(DelegatedTaskResult)> callback) {
  // If an existing task is running, do not execute this task.
  if (completion_callback_) {
    ReturnTaskCompletionStatusAsync(DelegatedTaskStatus::kRunnerBusy,
                                    base::TimeDelta(), std::move(callback));
    return;
  }

  task_start_time_ = base::TimeTicks::Now();
  completion_callback_ = std::move(callback);

  // TODO(b/525018453): Verify the binary after fetching the path.
  base::FilePath peh_binary_path = peh_launcher_->GetBinaryPath();
  if (peh_binary_path.empty()) {
    CleanupAndReturnResult(DelegatedTaskStatus::kPehNotFound);
    return;
  }

  base::CommandLine cmd_line = base::CommandLine(peh_binary_path);
  cmd_line.AppendSwitchASCII(kDelegatedTasksSwitch, task.GetTaskName());

  task.AppendCommandLineSwitches(cmd_line);

  // TODO(b/525019455): Implement logic for waiting for the process to complete.
  base::Process process =
      peh_launcher_->LaunchProcess(cmd_line, base::LaunchOptions());
  if (!process.IsValid()) {
    CleanupAndReturnResult(DelegatedTaskStatus::kProcessLaunchFailure);
    return;
  }

  CleanupAndReturnResult(DelegatedTaskStatus::kSuccess);
}

void DelegatedTaskRunner::CleanupAndReturnResult(DelegatedTaskStatus status) {
  CHECK(completion_callback_);

  base::TimeDelta execution_time = base::TimeTicks::Now() - task_start_time_;
  ReturnTaskCompletionStatusAsync(status, execution_time,
                                  std::move(completion_callback_));

  // TODO(b/525017787): Add UMA telemetry to log task result.
}

}  // namespace platform_experience
