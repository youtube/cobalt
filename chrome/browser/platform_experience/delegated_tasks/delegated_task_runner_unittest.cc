// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/platform_experience/delegated_tasks/delegated_task_runner.h"

#include <windows.h>

#include <memory>
#include <utility>
#include <vector>

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/functional/callback_helpers.h"
#include "base/process/launch.h"
#include "base/process/process.h"
#include "base/test/multiprocess_test.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/threading/platform_thread.h"
#include "chrome/browser/platform_experience/delegated_tasks/delegated_task.h"
#include "chrome/browser/platform_experience/delegated_tasks/peh_launcher.h"
#include "chrome/browser/platform_experience/delegated_tasks/peh_switches.h"
#include "chrome/browser/platform_experience/delegated_tasks/test_support/mock_peh_launcher.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/multiprocess_func_list.h"

using ::testing::_;
using ::testing::Return;

namespace platform_experience {

namespace {

const base::FilePath::CharType kFakeBinaryPath[] =
    FILE_PATH_LITERAL("C:\\path\\to\\fake_binary.exe");

class TestDelegatedTask : public DelegatedTask {
 public:
  TestDelegatedTask() = default;
  ~TestDelegatedTask() override = default;

  DelegatedTaskType GetTaskType() const override {
    return DelegatedTaskType::kRegisterSearchPromotion;
  }
  base::TimeDelta GetTimeout() const override { return base::Seconds(5); }

  DelegatedTaskStatus ParseExitCode(int exit_code) const override {
    if (exit_code == -1) {
      return DelegatedTaskStatus::kSuccess;
    }
    return DelegatedTaskStatus::kInvalidExitCode;
  }

  void AppendCommandLineSwitches(base::CommandLine& cmd_line) const override {
    cmd_line.AppendSwitchASCII("post-install-url", "https://example.com");
  }

  std::string_view GetTaskName() const override { return "TestTask"; }
};

class DelegatedTaskRunnerTest : public base::MultiProcessTest {
 protected:
  base::test::TaskEnvironment task_environment_;
};

class DelegatedTaskRunnerMockTimeTest : public base::MultiProcessTest {
 protected:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
};

MULTIPROCESS_TEST_MAIN(SuccessProcess) {
  return 0;
}

MULTIPROCESS_TEST_MAIN(InvalidTaskProcess) {
  return 1;
}

MULTIPROCESS_TEST_MAIN(TaskCustomExitCodeProcess) {
  return -1;
}

MULTIPROCESS_TEST_MAIN(TimeoutProcess) {
  // Sleep for longer than the task timeout (5 seconds).
  base::PlatformThread::Sleep(base::Seconds(10));
  return 0;
}

}  // namespace

TEST_F(DelegatedTaskRunnerTest, BinaryNotFound) {
  auto mock_launcher = std::make_unique<MockPehLauncher>();
  EXPECT_CALL(*mock_launcher, GetBinaryPath())
      .WillOnce(Return(base::FilePath()));
  EXPECT_CALL(*mock_launcher, LaunchProcess(_, _)).Times(0);

  DelegatedTaskRunner runner(std::move(mock_launcher));
  std::unique_ptr<DelegatedTask> task = std::make_unique<TestDelegatedTask>();
  base::test::TestFuture<DelegatedTaskResult> future;

  runner.Run(std::move(task), future.GetCallback());

  EXPECT_EQ(future.Get().status, DelegatedTaskStatus::kPehNotFound);
}

TEST_F(DelegatedTaskRunnerTest, ProcessLaunchFailure) {
  auto mock_launcher = std::make_unique<MockPehLauncher>();
  EXPECT_CALL(*mock_launcher, GetBinaryPath())
      .WillOnce(Return(base::FilePath(kFakeBinaryPath)));
  EXPECT_CALL(*mock_launcher, LaunchProcess(_, _))
      .WillOnce(Return(base::Process()));

  DelegatedTaskRunner runner(std::move(mock_launcher));
  std::unique_ptr<DelegatedTask> task = std::make_unique<TestDelegatedTask>();
  base::test::TestFuture<DelegatedTaskResult> future;

  runner.Run(std::move(task), future.GetCallback());

  EXPECT_EQ(future.Get().status, DelegatedTaskStatus::kProcessLaunchFailure);
}

TEST_F(DelegatedTaskRunnerTest, SuccessAndCommandLineVerification) {
  auto mock_launcher = std::make_unique<MockPehLauncher>();
  EXPECT_CALL(*mock_launcher, GetBinaryPath())
      .WillOnce(Return(base::FilePath(kFakeBinaryPath)));

  base::CommandLine launched_cmd_line(base::CommandLine::NO_PROGRAM);
  EXPECT_CALL(*mock_launcher, LaunchProcess(_, _))
      .WillOnce([&](const base::CommandLine& cmd_line,
                    const base::LaunchOptions& options) {
        launched_cmd_line = cmd_line;
        return base::SpawnMultiProcessTestChild(
            "SuccessProcess", base::GetMultiProcessTestChildBaseCommandLine(),
            options);
      });

  DelegatedTaskRunner runner(std::move(mock_launcher));
  std::unique_ptr<DelegatedTask> task = std::make_unique<TestDelegatedTask>();

  std::string expected_task_name(task->GetTaskName());

  base::test::TestFuture<DelegatedTaskResult> future;
  runner.Run(std::move(task), future.GetCallback());

  EXPECT_EQ(future.Get().status, DelegatedTaskStatus::kSuccess);

  EXPECT_EQ(launched_cmd_line.GetProgram(), base::FilePath(kFakeBinaryPath));
  EXPECT_EQ(launched_cmd_line.GetSwitchValueASCII(kDelegatedTasksSwitch),
            expected_task_name);

  EXPECT_EQ(launched_cmd_line.GetSwitchValueASCII("post-install-url"),
            "https://example.com");
}

TEST_F(DelegatedTaskRunnerTest, InvalidTask) {
  auto mock_launcher = std::make_unique<MockPehLauncher>();
  EXPECT_CALL(*mock_launcher, GetBinaryPath())
      .WillOnce(Return(base::FilePath(kFakeBinaryPath)));

  EXPECT_CALL(*mock_launcher, LaunchProcess(_, _))
      .WillOnce([&](const base::CommandLine& cmd_line,
                    const base::LaunchOptions& options) {
        return base::SpawnMultiProcessTestChild(
            "InvalidTaskProcess",
            base::GetMultiProcessTestChildBaseCommandLine(), options);
      });

  DelegatedTaskRunner runner(std::move(mock_launcher));
  std::unique_ptr<DelegatedTask> task = std::make_unique<TestDelegatedTask>();
  base::test::TestFuture<DelegatedTaskResult> future;

  runner.Run(std::move(task), future.GetCallback());

  EXPECT_EQ(future.Get().status, DelegatedTaskStatus::kInvalidTaskType);
}

TEST_F(DelegatedTaskRunnerTest, CustomExitCode) {
  auto mock_launcher = std::make_unique<MockPehLauncher>();
  EXPECT_CALL(*mock_launcher, GetBinaryPath())
      .WillOnce(Return(base::FilePath(kFakeBinaryPath)));

  EXPECT_CALL(*mock_launcher, LaunchProcess(_, _))
      .WillOnce([&](const base::CommandLine& cmd_line,
                    const base::LaunchOptions& options) {
        return base::SpawnMultiProcessTestChild(
            "TaskCustomExitCodeProcess",
            base::GetMultiProcessTestChildBaseCommandLine(), options);
      });

  DelegatedTaskRunner runner(std::move(mock_launcher));
  std::unique_ptr<DelegatedTask> task = std::make_unique<TestDelegatedTask>();
  base::test::TestFuture<DelegatedTaskResult> future;

  runner.Run(std::move(task), future.GetCallback());

  EXPECT_EQ(future.Get().status, DelegatedTaskStatus::kSuccess);
}

TEST_F(DelegatedTaskRunnerMockTimeTest, Timeout) {
  auto mock_launcher = std::make_unique<MockPehLauncher>();
  EXPECT_CALL(*mock_launcher, GetBinaryPath())
      .WillOnce(Return(base::FilePath(kFakeBinaryPath)));

  EXPECT_CALL(*mock_launcher, LaunchProcess(_, _))
      .WillOnce([&](const base::CommandLine& cmd_line,
                    const base::LaunchOptions& options) {
        return base::SpawnMultiProcessTestChild(
            "TimeoutProcess", base::GetMultiProcessTestChildBaseCommandLine(),
            options);
      });

  DelegatedTaskRunner runner(std::move(mock_launcher));
  std::unique_ptr<DelegatedTask> task = std::make_unique<TestDelegatedTask>();
  base::test::TestFuture<DelegatedTaskResult> future;

  runner.Run(std::move(task), future.GetCallback());

  // Fast-forward mock time to trigger the timeout.
  task_environment_.FastForwardBy(base::Seconds(10));

  EXPECT_EQ(future.Get().status, DelegatedTaskStatus::kTaskTimeout);
}

TEST_F(DelegatedTaskRunnerTest, RunnerDestroyedBeforeTaskCompletion) {
  auto mock_launcher = std::make_unique<MockPehLauncher>();
  EXPECT_CALL(*mock_launcher, GetBinaryPath())
      .WillOnce(Return(base::FilePath(kFakeBinaryPath)));

  EXPECT_CALL(*mock_launcher, LaunchProcess(_, _))
      .WillOnce([&](const base::CommandLine& cmd_line,
                    const base::LaunchOptions& options) {
        return base::SpawnMultiProcessTestChild(
            "TimeoutProcess", base::GetMultiProcessTestChildBaseCommandLine(),
            options);
      });

  base::test::TestFuture<DelegatedTaskResult> future;

  {
    DelegatedTaskRunner runner(std::move(mock_launcher));
    std::unique_ptr<DelegatedTask> task = std::make_unique<TestDelegatedTask>();

    runner.Run(std::move(task), future.GetCallback());
  }

  EXPECT_EQ(future.Get().status,
            DelegatedTaskStatus::kRunnerDestroyedBeforeTaskCompletion);
}

}  // namespace platform_experience
