// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/reporting/client/report_queue_provider.h"

#include <memory>
#include <string>
#include <utility>

#include "base/functional/bind.h"
#include "base/task/bind_post_task.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "components/reporting/client/mock_report_queue.h"
#include "components/reporting/client/mock_report_queue_provider.h"
#include "components/reporting/client/report_queue.h"
#include "components/reporting/client/report_queue_configuration.h"
#include "components/reporting/client/report_queue_provider_test_helper.h"
#include "components/reporting/util/statusor.h"
#include "components/reporting/util/test_support_callbacks.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::Eq;
using ::testing::Invoke;
using ::testing::NiceMock;
using ::testing::StrEq;
using ::testing::WithArg;

namespace reporting {
namespace {

constexpr char kTestMessage[] = "TEST MESSAGE";

class ReportQueueProviderTest : public ::testing::Test {
 protected:
  void SetUp() override {
    provider_ = std::make_unique<NiceMock<MockReportQueueProvider>>();
    report_queue_provider_test_helper::SetForTesting(provider_.get());
  }

  void TearDown() override {
    task_environment_.RunUntilIdle();  // Drain remaining scheduled tasks.
    report_queue_provider_test_helper::SetForTesting(nullptr);
  }

  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};

  std::unique_ptr<MockReportQueueProvider> provider_;
  const Destination destination_ = Destination::UPLOAD_EVENTS;
  ReportQueueConfiguration::PolicyCheckCallback policy_checker_callback_ =
      base::BindRepeating([]() { return Status::StatusOK(); });
};

// Asynchronously creates ReportingQueue and enqueue message.
// Returns result with callback.
void CreateQueuePostData(
    std::string data,
    Priority priority,
    std::unique_ptr<ReportQueueConfiguration> config,
    scoped_refptr<base::SequencedTaskRunner> sequenced_task_runner,
    ReportQueue::EnqueueCallback done_cb) {
  base::OnceCallback<void(StatusOr<std::unique_ptr<ReportQueue>>)> queue_cb =
      base::BindOnce(
          [](std::string data, Priority priority,
             ReportQueue::EnqueueCallback done_cb,
             StatusOr<std::unique_ptr<ReportQueue>> report_queue_result) {
            // Bail out if queue failed to create.
            if (!report_queue_result.ok()) {
              std::move(done_cb).Run(report_queue_result.status());
              return;
            }
            // Queue created successfully, enqueue the message on a random
            // thread and verify.
            EXPECT_CALL(*static_cast<MockReportQueue*>(
                            report_queue_result.ValueOrDie().get()),
                        AddRecord(StrEq(data), Eq(priority), _))
                .WillOnce(
                    WithArg<2>(Invoke([](ReportQueue::EnqueueCallback cb) {
                      std::move(cb).Run(Status::StatusOK());
                    })));
            base::ThreadPool::PostTask(
                FROM_HERE,
                base::BindOnce(
                    [](std::unique_ptr<ReportQueue> queue, std::string data,
                       Priority priority,
                       ReportQueue::EnqueueCallback done_cb) {
                      queue->Enqueue(data, priority, std::move(done_cb));
                    },
                    std::move(report_queue_result.ValueOrDie()), data, priority,
                    std::move(done_cb)));
          },
          std::string(data), priority, std::move(done_cb));
  ReportQueueProvider::CreateQueue(
      std::move(config),
      // Verification callback needs to be serialized, because EXPECT_... do not
      // support multithreading.
      base::BindPostTask(sequenced_task_runner, std::move(queue_cb)));
}

// Asynchronously creates ReportingQueue and enqueue message.
// Returns result with callback.
void CreateSpeculativeQueuePostData(
    std::string data,
    Priority priority,
    std::unique_ptr<ReportQueueConfiguration> config,
    scoped_refptr<base::SequencedTaskRunner> sequenced_task_runner,
    ReportQueue::EnqueueCallback done_cb) {
  auto report_queue_result =
      ReportQueueProvider::CreateSpeculativeQueue(std::move(config));
  // Bail out if queue failed to create.
  if (!report_queue_result.ok()) {
    std::move(done_cb).Run(report_queue_result.status());
    return;
  }
  // Queue created successfully, enqueue the message on a random thread and
  // verify.
  EXPECT_CALL(
      *static_cast<MockReportQueue*>(report_queue_result.ValueOrDie().get()),
      AddRecord(StrEq(data), Eq(priority), _))
      .WillOnce(WithArg<2>(Invoke([](ReportQueue::EnqueueCallback cb) {
        std::move(cb).Run(Status::StatusOK());
      })));
  // Enqueue on a random thread again.
  base::ThreadPool::PostTask(
      FROM_HERE,
      base::BindOnce(
          [](std::unique_ptr<ReportQueue, base::OnTaskRunnerDeleter> queue,
             std::string data, Priority priority,
             ReportQueue::EnqueueCallback done_cb) {
            queue->Enqueue(data, priority, std::move(done_cb));
          },
          std::move(report_queue_result.ValueOrDie()), data, priority,
          // Verification callback needs to be serialized, because EXPECT_... do
          // not support multithreading.
          base::BindPostTask(sequenced_task_runner, std::move(done_cb))));
}

TEST_F(ReportQueueProviderTest, CreateAndGetQueue) {
  // Create configuration.
  auto config_result = ReportQueueConfiguration::Create(
      EventType::kDevice, destination_, policy_checker_callback_);
  ASSERT_OK(config_result);
  EXPECT_CALL(*provider_.get(), OnInitCompletedMock()).Times(1);
  provider_->ExpectCreateNewQueueAndReturnNewMockQueue(1);
  // Use it to asynchronously create ReportingQueue and then asynchronously
  // send the message.
  test::TestEvent<Status> e;
  base::ThreadPool::PostTask(
      FROM_HERE,
      base::BindOnce(&CreateQueuePostData, kTestMessage, FAST_BATCH,
                     std::move(config_result.ValueOrDie()),
                     base::SequencedTaskRunner::GetCurrentDefault(), e.cb()));
  const auto res = e.result();
  EXPECT_OK(res) << res;
}

TEST_F(ReportQueueProviderTest, CreateMultipleQueues) {
  static constexpr std::array<std::pair<Priority, Destination>, 9> send_as{
      std::make_pair(FAST_BATCH, HEARTBEAT_EVENTS),
      std::make_pair(SLOW_BATCH, CRD_EVENTS),
      std::make_pair(IMMEDIATE, LOGIN_LOGOUT_EVENTS),
      std::make_pair(SLOW_BATCH, HEARTBEAT_EVENTS),
      std::make_pair(FAST_BATCH, CRD_EVENTS),
      std::make_pair(IMMEDIATE, LOGIN_LOGOUT_EVENTS),
      std::make_pair(FAST_BATCH, HEARTBEAT_EVENTS),
      std::make_pair(IMMEDIATE, CRD_EVENTS),
      std::make_pair(SLOW_BATCH, LOGIN_LOGOUT_EVENTS),
  };
  test::TestCallbackAutoWaiter waiter;
  waiter.Attach(send_as.size());
  // Expect only one InitCompleted callback.
  EXPECT_CALL(*provider_.get(), OnInitCompletedMock()).Times(1);
  // ... even though we create multiple queues.
  provider_->ExpectCreateNewQueueAndReturnNewMockQueue(send_as.size());
  for (const auto& s : send_as) {
    // Create configuration.
    auto config_result = ReportQueueConfiguration::Create(
        EventType::kDevice, /*destination=*/s.second, policy_checker_callback_);
    ASSERT_OK(config_result);
    // Compose the message.
    std::string message = std::string(kTestMessage)
                              .append(" priority=")
                              .append(Priority_Name(s.first))
                              .append(" destination=")
                              .append(Destination_Name(s.second));
    // Asynchronously create ReportingQueue and then asynchronously send the
    // message.
    auto done_cb = base::BindOnce(
        [](test::TestCallbackWaiter* waiter, Status status) {
          EXPECT_OK(status) << status;
          waiter->Signal();
        },
        &waiter);
    base::ThreadPool::PostTask(
        FROM_HERE,
        base::BindOnce(&CreateQueuePostData, std::move(message),
                       /*priority=*/s.first,
                       std::move(config_result.ValueOrDie()),
                       base::SequencedTaskRunner::GetCurrentDefault(),
                       std::move(done_cb)));
  }
  waiter.Signal();  // Release the waiter
}

TEST_F(ReportQueueProviderTest, CreateMultipleSpeculativeQueues) {
  static constexpr std::array<std::pair<Priority, Destination>, 9> send_as{
      std::make_pair(FAST_BATCH, HEARTBEAT_EVENTS),
      std::make_pair(SLOW_BATCH, CRD_EVENTS),
      std::make_pair(IMMEDIATE, LOGIN_LOGOUT_EVENTS),
      std::make_pair(SLOW_BATCH, HEARTBEAT_EVENTS),
      std::make_pair(FAST_BATCH, CRD_EVENTS),
      std::make_pair(IMMEDIATE, LOGIN_LOGOUT_EVENTS),
      std::make_pair(FAST_BATCH, HEARTBEAT_EVENTS),
      std::make_pair(IMMEDIATE, CRD_EVENTS),
      std::make_pair(SLOW_BATCH, LOGIN_LOGOUT_EVENTS),
  };
  test::TestCallbackAutoWaiter waiter;
  waiter.Attach(send_as.size());
  // Expect only one InitCompleted callback.
  EXPECT_CALL(*provider_.get(), OnInitCompletedMock()).Times(1);
  // ... even though we create multiple queues.
  provider_->ExpectCreateNewSpeculativeQueueAndReturnNewMockQueue(
      send_as.size());
  for (const auto& s : send_as) {
    // Create configuration.
    auto config_result = ReportQueueConfiguration::Create(
        EventType::kDevice, /*destination=*/s.second, policy_checker_callback_);
    ASSERT_OK(config_result);
    // Compose the message.
    std::string message = std::string(kTestMessage)
                              .append(" priority=")
                              .append(Priority_Name(s.first))
                              .append(" destination=")
                              .append(Destination_Name(s.second));
    // Create SpeculativeReportingQueue and then asynchronously send the
    // message.
    auto done_cb = base::BindOnce(
        [](test::TestCallbackWaiter* waiter, Status status) {
          EXPECT_OK(status) << status;
          waiter->Signal();
        },
        &waiter);
    CreateSpeculativeQueuePostData(
        std::move(message),
        /*priority=*/s.first, std::move(config_result.ValueOrDie()),
        base::SequencedTaskRunner::GetCurrentDefault(), std::move(done_cb));
  }
  waiter.Signal();  // Release the waiter
}

TEST_F(ReportQueueProviderTest,
       CreateReportQueueWithEncryptedReportingPipelineDisabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(kEncryptedReportingPipeline);

  // Create configuration
  auto config_result = ReportQueueConfiguration::Create(
      EventType::kDevice, destination_, policy_checker_callback_);
  ASSERT_OK(config_result);

  test::TestEvent<ReportQueueProvider::CreateReportQueueResponse> event;
  ReportQueueProvider::CreateQueue(std::move(config_result.ValueOrDie()),
                                   event.cb());
  const auto result = event.result();

  ASSERT_FALSE(result.ok());
  EXPECT_EQ(result.status().code(), error::FAILED_PRECONDITION);
}

TEST_F(ReportQueueProviderTest,
       CreateSpeculativeReportQueueWithEncryptedReportingPipelineDisabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(kEncryptedReportingPipeline);

  // Create configuration
  auto config_result = ReportQueueConfiguration::Create(
      EventType::kDevice, destination_, policy_checker_callback_);
  ASSERT_OK(config_result);

  const auto result = ReportQueueProvider::CreateSpeculativeQueue(
      std::move(config_result.ValueOrDie()));
  ASSERT_FALSE(result.ok());
  EXPECT_EQ(result.status().code(), error::FAILED_PRECONDITION);
}
}  // namespace
}  // namespace reporting
