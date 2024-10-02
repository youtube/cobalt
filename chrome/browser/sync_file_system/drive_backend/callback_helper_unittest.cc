// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync_file_system/drive_backend/callback_helper.h"

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/location.h"
#include "base/run_loop.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/task_environment.h"
#include "base/threading/thread.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace sync_file_system {
namespace drive_backend {

namespace {

void SimpleCallback(bool* called, int) {
  ASSERT_TRUE(called);
  EXPECT_FALSE(*called);
  *called = true;
}

void CallbackWithPassed(bool* called, std::unique_ptr<int>) {
  ASSERT_TRUE(called);
  EXPECT_FALSE(*called);
  *called = true;
}

void VerifyCalledOnTaskRunner(base::SingleThreadTaskRunner* task_runner,
                              bool* called) {
  ASSERT_TRUE(called);
  ASSERT_TRUE(task_runner);

  EXPECT_TRUE(task_runner->RunsTasksInCurrentSequence());
  EXPECT_FALSE(*called);
  *called = true;
}

}  // namespace

TEST(DriveBackendCallbackHelperTest, BasicTest) {
  base::test::SingleThreadTaskEnvironment task_environment;

  bool called = false;
  RelayCallbackToCurrentThread(FROM_HERE,
                               base::BindOnce(&SimpleCallback, &called))
      .Run(0);
  EXPECT_FALSE(called);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(called);

  called = false;
  RelayCallbackToCurrentThread(FROM_HERE,
                               base::BindOnce(&CallbackWithPassed, &called))
      .Run(std::unique_ptr<int>(new int));
  EXPECT_FALSE(called);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(called);
}

TEST(DriveBackendCallbackHelperTest, RunOnOtherThreadTest) {
  base::test::SingleThreadTaskEnvironment task_environment;
  base::Thread thread("WorkerThread");
  thread.Start();

  scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner =
      base::SingleThreadTaskRunner::GetCurrentDefault();
  scoped_refptr<base::SequencedTaskRunner> worker_task_runner =
      thread.task_runner();

  bool called = false;
  base::RunLoop run_loop;
  worker_task_runner->PostTask(
      FROM_HERE,
      RelayCallbackToTaskRunner(
          ui_task_runner.get(), FROM_HERE,
          base::BindOnce(&VerifyCalledOnTaskRunner,
                         base::RetainedRef(ui_task_runner), &called)));
  worker_task_runner->PostTask(
      FROM_HERE,
      RelayCallbackToTaskRunner(
          ui_task_runner.get(), FROM_HERE, run_loop.QuitClosure()));
  run_loop.Run();
  EXPECT_TRUE(called);

  thread.Stop();
  base::RunLoop().RunUntilIdle();
}

TEST(DriveBackendCallbackHelperTest, PassNullFunctionTest) {
  base::test::SingleThreadTaskEnvironment task_environment;
  base::OnceClosure closure =
      RelayCallbackToCurrentThread(FROM_HERE, base::OnceClosure());
  EXPECT_TRUE(closure.is_null());
}

}  // namespace drive_backend
}  // namespace sync_file_system
