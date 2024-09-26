// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/chrome_cleaner/chrome_cleaner_runner_win.h"

#include <memory>
#include <string>
#include <utility>

#include "base/base_paths.h"
#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/metrics/histogram_functions.h"
#include "base/path_service.h"
#include "base/process/launch.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/trace_event/trace_event.h"
#include "chrome/browser/safe_browsing/chrome_cleaner/chrome_cleaner_scanner_results_win.h"
#include "chrome/browser/safe_browsing/chrome_cleaner/chrome_prompt_actions_win.h"
#include "chrome/browser/safe_browsing/chrome_cleaner/chrome_prompt_channel_win.h"
#include "chrome/browser/safe_browsing/chrome_cleaner/srt_client_info_win.h"
#include "chrome/browser/safe_browsing/chrome_cleaner/srt_field_trial_win.h"
#include "chrome/browser/safe_browsing/chrome_cleaner/sw_reporter_invocation_win.h"
#include "chrome/installer/util/install_util.h"
#include "components/chrome_cleaner/public/constants/constants.h"
#include "components/version_info/version_info.h"

namespace safe_browsing {

namespace {

// Global delegate used to override the launching of the Cleaner process during
// tests.
ChromeCleanerRunnerTestDelegate* g_test_delegate = nullptr;

}  // namespace

ChromeCleanerRunner::ProcessStatus::ProcessStatus(LaunchStatus launch_status,
                                                  int exit_code)
    : launch_status(launch_status), exit_code(exit_code) {}

// static
void ChromeCleanerRunner::RunChromeCleanerAndReplyWithExitCode(
    const base::FilePath& cleaner_executable_path,
    const SwReporterInvocation& reporter_invocation,
    ChromeMetricsStatus metrics_status,
    ChromePromptActions::PromptUserCallback on_prompt_user,
    ConnectionClosedCallback on_connection_closed,
    ProcessDoneCallback on_process_done,
    scoped_refptr<base::SequencedTaskRunner> task_runner) {
  auto cleaner_runner = base::WrapRefCounted(new ChromeCleanerRunner(
      cleaner_executable_path, reporter_invocation, metrics_status,
      std::move(on_prompt_user), std::move(on_connection_closed),
      std::move(on_process_done), std::move(task_runner)));
  auto launch_and_wait = base::BindOnce(
      &ChromeCleanerRunner::LaunchAndWaitForExitOnBackgroundThread,
      cleaner_runner);
  auto process_done =
      base::BindOnce(&ChromeCleanerRunner::OnProcessDone, cleaner_runner);
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE,
      // LaunchAndWaitForExitOnBackgroundThread creates (MayBlock()) and joins
      // (WithBaseSyncPrimitives()) a process.
      {base::MayBlock(), base::WithBaseSyncPrimitives(),
       base::TaskPriority::BEST_EFFORT,
       base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN},
      std::move(launch_and_wait), std::move(process_done));
}

ChromeCleanerRunner::ChromeCleanerRunner(
    const base::FilePath& cleaner_executable_path,
    const SwReporterInvocation& reporter_invocation,
    ChromeMetricsStatus metrics_status,
    ChromePromptActions::PromptUserCallback on_prompt_user,
    ConnectionClosedCallback on_connection_closed,
    ProcessDoneCallback on_process_done,
    scoped_refptr<base::SequencedTaskRunner> task_runner)
    : task_runner_(std::move(task_runner)),
      reporter_invocation_(reporter_invocation),
      metrics_status_(metrics_status),
      cleaner_executable_path_(cleaner_executable_path),
      on_prompt_user_(std::move(on_prompt_user)),
      on_connection_closed_(std::move(on_connection_closed)),
      on_process_done_(std::move(on_process_done)) {
  DCHECK(on_prompt_user_);
  DCHECK(on_connection_closed_);
  DCHECK(on_process_done_);
  DCHECK(!cleaner_executable_path.empty());
}

base::CommandLine
ChromeCleanerRunner::ConstructCommandLineOnBackgroundThread() {
  base::CommandLine cleaner_command_line(cleaner_executable_path_);
  // Add the non-IPC switches that should be passed to the Cleaner process.

  // Add switches that pass information about this Chrome installation.
  cleaner_command_line.AppendSwitchASCII(chrome_cleaner::kChromeVersionSwitch,
                                         version_info::GetVersionNumber());
  cleaner_command_line.AppendSwitchASCII(chrome_cleaner::kChromeChannelSwitch,
                                         base::NumberToString(ChannelAsInt()));
  base::FilePath chrome_exe_path;
  base::PathService::Get(base::FILE_EXE, &chrome_exe_path);
  cleaner_command_line.AppendSwitchPath(chrome_cleaner::kChromeExePathSwitch,
                                        chrome_exe_path);
  if (!InstallUtil::IsPerUserInstall())
    cleaner_command_line.AppendSwitch(
        chrome_cleaner::kChromeSystemInstallSwitch);

  // Start the cleaner process in scanning mode.
  cleaner_command_line.AppendSwitchASCII(
      chrome_cleaner::kExecutionModeSwitch,
      base::NumberToString(
          static_cast<int>(chrome_cleaner::ExecutionMode::kScanning)));

  // If set, forward the engine flag from the reporter. Otherwise, set the
  // engine flag explicitly to 1.
  const std::string& reporter_engine =
      reporter_invocation_.command_line().GetSwitchValueASCII(
          chrome_cleaner::kEngineSwitch);
  cleaner_command_line.AppendSwitchASCII(
      chrome_cleaner::kEngineSwitch,
      reporter_engine.empty() ? "1" : reporter_engine);

  if (reporter_invocation_.cleaner_logs_upload_enabled()) {
    cleaner_command_line.AppendSwitch(
        chrome_cleaner::kWithScanningModeLogsSwitch);
  }

  cleaner_command_line.AppendSwitchASCII(
      chrome_cleaner::kChromePromptSwitch,
      base::NumberToString(
          static_cast<int>(reporter_invocation_.chrome_prompt())));

  // If metrics is enabled, we can enable crash reporting in the Chrome Cleaner
  // process.
  if (metrics_status_ == ChromeMetricsStatus::kEnabled) {
    cleaner_command_line.AppendSwitch(chrome_cleaner::kUmaUserSwitch);
    cleaner_command_line.AppendSwitch(
        chrome_cleaner::kEnableCrashReportingSwitch);
  }

  // Older versions of the Chrome Cleanup Tool needs this switch to ensure
  // resetting of shortcuts.
  cleaner_command_line.AppendSwitch(chrome_cleaner::kResetShortcutsSwitch);

  return cleaner_command_line;
}

ChromeCleanerRunner::ProcessStatus
ChromeCleanerRunner::LaunchAndWaitForExitOnBackgroundThread() {
  TRACE_EVENT0("safe_browsing",
               "ChromeCleanerRunner::LaunchAndWaitForExitOnBackgroundThread");
  auto on_connection_closed =
      base::BindOnce(&ChromeCleanerRunner::OnConnectionClosed, this);
  auto actions = std::make_unique<ChromePromptActions>(
      base::BindOnce(&ChromeCleanerRunner::OnPromptUser, this));

  // The channel will make blocking calls to ::WriteFile.
  scoped_refptr<base::SequencedTaskRunner> channel_task_runner =
      base::ThreadPool::CreateSequencedTaskRunner({base::MayBlock()});

  // ChromePromptChannel method calls will be posted to this sequence using
  // WeakPtr's, so the channel must be deleted on the same sequence.
  std::unique_ptr<ChromePromptChannel, base::OnTaskRunnerDeleter> channel(
      new ChromePromptChannel(std::move(on_connection_closed),
                              std::move(actions), channel_task_runner),
      base::OnTaskRunnerDeleter(channel_task_runner));

  base::CommandLine cleaner_command_line =
      ConstructCommandLineOnBackgroundThread();

  base::LaunchOptions launch_options;
  if (!channel->PrepareForCleaner(cleaner_command_line,
                                  &launch_options.handles_to_inherit)) {
    return ProcessStatus(LaunchStatus::kLaunchFailed);
  }

  base::Process cleaner_process =
      g_test_delegate
          ? g_test_delegate->LaunchTestProcess(cleaner_command_line,
                                               launch_options)
          : base::LaunchProcess(cleaner_command_line, launch_options);
  if (!cleaner_process.IsValid()) {
    channel->CleanupAfterCleanerLaunchFailed();
    return ProcessStatus(LaunchStatus::kLaunchFailed);
  }
  channel->ConnectToCleaner(
      ChromePromptChannel::CreateDelegateForProcess(cleaner_process));

  int exit_code = -1;
  if (!cleaner_process.WaitForExit(&exit_code)) {
    return ProcessStatus(
        LaunchStatus::kLaunchSucceededFailedToWaitForCompletion);
  }

  return ProcessStatus(LaunchStatus::kSuccess, exit_code);
}

ChromeCleanerRunner::~ChromeCleanerRunner() = default;

void ChromeCleanerRunner::OnPromptUser(
    ChromeCleanerScannerResults&& scanner_results,
    ChromePromptActions::PromptUserReplyCallback reply_callback) {
  if (on_prompt_user_) {
    task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(on_prompt_user_), std::move(scanner_results),
                       std::move(reply_callback)));
  }
}

void ChromeCleanerRunner::OnConnectionClosed() {
  if (on_connection_closed_)
    task_runner_->PostTask(FROM_HERE, std::move(on_connection_closed_));
}

void ChromeCleanerRunner::OnProcessDone(ProcessStatus process_status) {
  if (g_test_delegate) {
    task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&ChromeCleanerRunnerTestDelegate::OnCleanerProcessDone,
                       base::Unretained(g_test_delegate), process_status));
  }

  if (on_process_done_) {
    task_runner_->PostTask(
        FROM_HERE, base::BindOnce(std::move(on_process_done_), process_status));
  }
}

void SetChromeCleanerRunnerTestDelegateForTesting(
    ChromeCleanerRunnerTestDelegate* test_delegate) {
  g_test_delegate = test_delegate;
}

}  // namespace safe_browsing
