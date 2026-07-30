// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/platform_experience/delegated_tasks/delegated_task_runner.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/functional/callback_helpers.h"
#include "base/process/launch.h"
#include "base/process/process.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "chrome/browser/platform_experience/delegated_tasks/delegated_task.h"
#include "chrome/browser/platform_experience/delegated_tasks/peh_launcher.h"
#include "chrome/browser/platform_experience/delegated_tasks/peh_switches.h"
#include "chrome/browser/platform_experience/delegated_tasks/test_support/mock_peh_launcher.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

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
  base::TimeDelta GetTimeout() const override { return base::Seconds(10); }
  DelegatedTaskStatus ParseExitCode(int exit_code) const override {
    return DelegatedTaskStatus::kSuccess;
  }

  void AppendCommandLineSwitches(base::CommandLine& cmd_line) const override {
    cmd_line.AppendSwitchASCII("post-install-url", "https://example.com");
  }

  std::string_view GetTaskName() const override { return "TestTask"; }
};

class DelegatedTaskRunnerTest : public testing::Test {
 protected:
  base::test::TaskEnvironment task_environment_;
};

TEST_F(DelegatedTaskRunnerTest, BinaryNotFound) {
  auto mock_launcher = std::make_unique<MockPehLauncher>();
  EXPECT_CALL(*mock_launcher, GetBinaryPath())
      .WillOnce(Return(base::FilePath()));
  EXPECT_CALL(*mock_launcher, LaunchProcess(_, _)).Times(0);

  DelegatedTaskRunner runner(std::move(mock_launcher));
  TestDelegatedTask task;
  base::test::TestFuture<DelegatedTaskResult> future;

  runner.Run(task, future.GetCallback());

  EXPECT_EQ(future.Get().status, DelegatedTaskStatus::kPehNotFound);
}

TEST_F(DelegatedTaskRunnerTest, ProcessLaunchFailure) {
  auto mock_launcher = std::make_unique<MockPehLauncher>();
  EXPECT_CALL(*mock_launcher, GetBinaryPath())
      .WillOnce(Return(base::FilePath(kFakeBinaryPath)));
  EXPECT_CALL(*mock_launcher, LaunchProcess(_, _))
      .WillOnce(Return(base::Process()));

  DelegatedTaskRunner runner(std::move(mock_launcher));
  TestDelegatedTask task;
  base::test::TestFuture<DelegatedTaskResult> future;

  runner.Run(task, future.GetCallback());

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
        return base::Process(base::Process::Current().Duplicate());
      });

  DelegatedTaskRunner runner(std::move(mock_launcher));
  TestDelegatedTask task;
  base::test::TestFuture<DelegatedTaskResult> future;

  runner.Run(task, future.GetCallback());

  EXPECT_EQ(future.Get().status, DelegatedTaskStatus::kSuccess);

  EXPECT_EQ(launched_cmd_line.GetProgram(), base::FilePath(kFakeBinaryPath));
  EXPECT_EQ(launched_cmd_line.GetSwitchValueASCII(kDelegatedTasksSwitch),
            task.GetTaskName());

  EXPECT_EQ(launched_cmd_line.GetSwitchValueASCII("post-install-url"),
            "https://example.com");
}

}  // namespace

}  // namespace platform_experience
