// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/win/setup/setup_util.h"

#include <memory>
#include <string>

#include "base/command_line.h"
#include "chrome/updater/test_scope.h"
#include "chrome/updater/updater_scope.h"
#include "chrome/updater/util/unittest_util.h"
#include "chrome/updater/win/task_scheduler.h"
#include "chrome/updater/win/test/test_executables.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace updater {

class SetupUtilRegisterWakeTaskWorkItemTests : public ::testing::Test {
 public:
  void SetUp() override {
    task_scheduler_ = TaskScheduler::CreateInstance(GetTestScope());
    ASSERT_TRUE(task_scheduler_);
  }

  void TearDown() override {
    std::wstring task_name(GetTaskName(GetTestScope()));
    EXPECT_TRUE(task_name.empty());
    while (!task_name.empty()) {
      EXPECT_TRUE(task_scheduler_->DeleteTask(task_name.c_str()));
      task_name = GetTaskName(GetTestScope());
    }
  }

 protected:
  scoped_refptr<TaskScheduler> task_scheduler_;
  const base::CommandLine command_line_ =
      GetTestProcessCommandLine(GetTestScope(), test::GetTestName());
};

TEST_F(SetupUtilRegisterWakeTaskWorkItemTests, TaskDoesNotExist) {
  ASSERT_TRUE(GetTaskName(GetTestScope()).empty());

  std::unique_ptr<WorkItemList> install_list(WorkItem::CreateWorkItemList());
  install_list->AddWorkItem(
      new RegisterWakeTaskWorkItem(command_line_, GetTestScope()));
  ASSERT_TRUE(install_list->Do());
  ASSERT_FALSE(GetTaskName(GetTestScope()).empty());

  install_list->Rollback();
  ASSERT_TRUE(GetTaskName(GetTestScope()).empty());
}

TEST_F(SetupUtilRegisterWakeTaskWorkItemTests, TaskAlreadyExists) {
  ASSERT_TRUE(GetTaskName(GetTestScope()).empty());

  std::unique_ptr<WorkItemList> install_list(WorkItem::CreateWorkItemList());
  install_list->AddWorkItem(
      new RegisterWakeTaskWorkItem(command_line_, GetTestScope()));
  ASSERT_TRUE(install_list->Do());
  ASSERT_FALSE(GetTaskName(GetTestScope()).empty());

  std::unique_ptr<WorkItemList> install_list_task_exists(
      WorkItem::CreateWorkItemList());
  install_list_task_exists->AddWorkItem(
      new RegisterWakeTaskWorkItem(command_line_, GetTestScope()));
  ASSERT_TRUE(install_list_task_exists->Do());
  ASSERT_FALSE(GetTaskName(GetTestScope()).empty());

  install_list_task_exists->Rollback();
  ASSERT_FALSE(GetTaskName(GetTestScope()).empty());

  install_list->Rollback();
  ASSERT_TRUE(GetTaskName(GetTestScope()).empty());
}

}  // namespace updater
