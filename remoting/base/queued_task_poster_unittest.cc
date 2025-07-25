// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/base/queued_task_poster.h"

#include <memory>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/ref_counted.h"
#include "base/run_loop.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/task_environment.h"
#include "base/threading/thread.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace remoting {

class QueuedTaskPosterTest : public testing::Test {
 public:
  QueuedTaskPosterTest();
  void SetUp() override;
  void TearDown() override;

 protected:
  base::OnceClosure SetSequenceStartedClosure(bool started);
  base::OnceClosure AssertExecutionOrderClosure(int order);
  base::OnceClosure AssertSequenceNotStartedClosure();

  void RunUntilPosterDone();

  scoped_refptr<base::SingleThreadTaskRunner> main_task_runner_;
  scoped_refptr<base::SingleThreadTaskRunner> target_task_runner_;
  std::unique_ptr<QueuedTaskPoster> poster_;
  int current_execution_order_ = 0;

 private:
  void SetSequenceStarted(bool started);
  void AssertExecutionOrder(int order);
  void AssertSequenceNotStarted();

  base::Thread target_thread_;
  base::test::SingleThreadTaskEnvironment task_environment_;
  bool sequence_started_ = false;
};

QueuedTaskPosterTest::QueuedTaskPosterTest()
    : target_thread_("Target Thread") {}

void QueuedTaskPosterTest::SetUp() {
  target_thread_.StartAndWaitForTesting();
  main_task_runner_ = base::SingleThreadTaskRunner::GetCurrentDefault();
  target_task_runner_ = target_thread_.task_runner();
  poster_ = std::make_unique<QueuedTaskPoster>(target_task_runner_);
}

void QueuedTaskPosterTest::TearDown() {
  target_thread_.Stop();
}

base::OnceClosure QueuedTaskPosterTest::SetSequenceStartedClosure(
    bool started) {
  return base::BindOnce(&QueuedTaskPosterTest::SetSequenceStarted,
                        base::Unretained(this), started);
}

base::OnceClosure QueuedTaskPosterTest::AssertExecutionOrderClosure(int order) {
  return base::BindOnce(&QueuedTaskPosterTest::AssertExecutionOrder,
                        base::Unretained(this), order);
}

base::OnceClosure QueuedTaskPosterTest::AssertSequenceNotStartedClosure() {
  return base::BindOnce(&QueuedTaskPosterTest::AssertSequenceNotStarted,
                        base::Unretained(this));
}

void QueuedTaskPosterTest::RunUntilPosterDone() {
  base::RunLoop run_loop;
  poster_->AddTask(base::BindOnce(
      base::IgnoreResult(&base::SingleThreadTaskRunner::PostTask),
      main_task_runner_, FROM_HERE, run_loop.QuitClosure()));
  run_loop.Run();
}

void QueuedTaskPosterTest::SetSequenceStarted(bool started) {
  sequence_started_ = started;
}

void QueuedTaskPosterTest::AssertExecutionOrder(int order) {
  ASSERT_EQ(current_execution_order_ + 1, order);
  current_execution_order_++;
}

void QueuedTaskPosterTest::AssertSequenceNotStarted() {
  ASSERT_FALSE(sequence_started_);
}

TEST_F(QueuedTaskPosterTest, TestTaskOrder) {
  poster_->AddTask(AssertExecutionOrderClosure(1));
  poster_->AddTask(AssertExecutionOrderClosure(2));
  poster_->AddTask(AssertExecutionOrderClosure(3));
  poster_->AddTask(AssertExecutionOrderClosure(4));
  poster_->AddTask(AssertExecutionOrderClosure(5));

  RunUntilPosterDone();
  EXPECT_EQ(5, current_execution_order_);
}

TEST_F(QueuedTaskPosterTest, TestTaskSequenceNotInterfered) {
  target_task_runner_->PostTask(FROM_HERE, AssertSequenceNotStartedClosure());
  poster_->AddTask(SetSequenceStartedClosure(true));
  target_task_runner_->PostTask(FROM_HERE, AssertSequenceNotStartedClosure());
  poster_->AddTask(AssertExecutionOrderClosure(1));
  target_task_runner_->PostTask(FROM_HERE, AssertSequenceNotStartedClosure());
  poster_->AddTask(AssertExecutionOrderClosure(2));
  target_task_runner_->PostTask(FROM_HERE, AssertSequenceNotStartedClosure());
  poster_->AddTask(AssertExecutionOrderClosure(3));
  target_task_runner_->PostTask(FROM_HERE, AssertSequenceNotStartedClosure());
  poster_->AddTask(AssertExecutionOrderClosure(4));
  target_task_runner_->PostTask(FROM_HERE, AssertSequenceNotStartedClosure());
  poster_->AddTask(AssertExecutionOrderClosure(5));
  target_task_runner_->PostTask(FROM_HERE, AssertSequenceNotStartedClosure());
  poster_->AddTask(SetSequenceStartedClosure(false));
  target_task_runner_->PostTask(FROM_HERE, AssertSequenceNotStartedClosure());

  RunUntilPosterDone();
  EXPECT_EQ(5, current_execution_order_);
}

TEST_F(QueuedTaskPosterTest, TestUsingPosterInMultipleTasks) {
  poster_->AddTask(AssertExecutionOrderClosure(1));
  poster_->AddTask(AssertExecutionOrderClosure(2));
  poster_->AddTask(AssertExecutionOrderClosure(3));

  RunUntilPosterDone();
  EXPECT_EQ(3, current_execution_order_);

  poster_->AddTask(AssertExecutionOrderClosure(4));
  poster_->AddTask(AssertExecutionOrderClosure(5));
  poster_->AddTask(AssertExecutionOrderClosure(6));

  RunUntilPosterDone();
  EXPECT_EQ(6, current_execution_order_);
}

}  // namespace remoting
