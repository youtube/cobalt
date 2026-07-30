// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/platform_experience/delegated_tasks/delegated_task_runner.h"

#include <windows.h>

#include <memory>
#include <utility>

#include "base/check.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/location.h"
#include "base/memory/weak_ptr.h"
#include "base/process/launch.h"
#include "base/process/process.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "base/win/windows_types.h"
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
  if (task_) {
    CleanupAndReturnResult(
        DelegatedTaskStatus::kRunnerDestroyedBeforeTaskCompletion);
  }
}

void DelegatedTaskRunner::Run(
    std::unique_ptr<DelegatedTask> task,
    base::OnceCallback<void(DelegatedTaskResult)> callback) {
  CHECK(task_start_time_.is_null());

  task_ = std::move(task);
  task_start_time_ = base::TimeTicks::Now();
  completion_callback_ = std::move(callback);

  // TODO(b/525018453): Verify the binary after fetching the path.
  base::FilePath peh_binary_path = peh_launcher_->GetBinaryPath();
  if (peh_binary_path.empty()) {
    CleanupAndReturnResult(DelegatedTaskStatus::kPehNotFound);
    return;
  }

  base::CommandLine cmd_line = base::CommandLine(peh_binary_path);
  cmd_line.AppendSwitchASCII(kDelegatedTasksSwitch, task_->GetTaskName());
  task_->AppendCommandLineSwitches(cmd_line);

  process_ = peh_launcher_->LaunchProcess(cmd_line, base::LaunchOptions());
  if (!process_.IsValid()) {
    CleanupAndReturnResult(DelegatedTaskStatus::kProcessLaunchFailure);
    return;
  }

  if (!watcher_.StartWatchingOnce(process_.Handle(), this)) {
    CleanupAndReturnResult(DelegatedTaskStatus::kWatchProcessHandleFailure);
    return;
  }

  base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&DelegatedTaskRunner::CleanupAndReturnResult,
                     weak_factory_.GetWeakPtr(),
                     DelegatedTaskStatus::kTaskTimeout),
      task_->GetTimeout());
}

void DelegatedTaskRunner::OnObjectSignaled(HANDLE object) {
  DWORD exit_code = 0;
  DelegatedTaskStatus status = DelegatedTaskStatus::kInvalidExitCode;

  if (::GetExitCodeProcess(process_.Handle(), &exit_code)) {
    switch (exit_code) {
      case 0:
        status = DelegatedTaskStatus::kSuccess;
        break;
      case 1:
        status = DelegatedTaskStatus::kInvalidTaskType;
        break;
      default:
        status = task_->ParseExitCode(static_cast<int>(exit_code));
    }
  } else {
    status = DelegatedTaskStatus::kInvalidExitCode;
  }

  CleanupAndReturnResult(status);
}

void DelegatedTaskRunner::CleanupAndReturnResult(DelegatedTaskStatus status) {
  watcher_.StopWatching();

  if (process_.IsValid() && process_.IsRunning()) {
    process_.Terminate(/*exit_code=*/1, /*wait=*/false);
  }

  weak_factory_.InvalidateWeakPtrs();
  task_.reset();

  CHECK(completion_callback_);
  base::TimeDelta execution_time = base::TimeTicks::Now() - task_start_time_;
  ReturnTaskCompletionStatusAsync(status, execution_time,
                                  std::move(completion_callback_));

  // TODO(b/525017787): Add UMA telemetry to log task result.
}

}  // namespace platform_experience
