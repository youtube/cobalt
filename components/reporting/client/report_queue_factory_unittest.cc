// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/reporting/client/report_queue_factory.h"

#include <memory>
#include <string>
#include <utility>

#include "base/logging.h"
#include "base/memory/weak_ptr.h"
#include "base/test/task_environment.h"
#include "components/reporting/client/mock_report_queue.h"
#include "components/reporting/client/mock_report_queue_provider.h"
#include "components/reporting/client/report_queue.h"
#include "components/reporting/client/report_queue_provider_test_helper.h"
#include "components/reporting/util/test_support_callbacks.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::HasSubstr;
using ::testing::IsNull;
using ::testing::NiceMock;
using ::testing::NotNull;
using ::testing::Return;

namespace reporting {

class MockReportQueueConsumer {
 public:
  MockReportQueueConsumer() = default;
  void SetReportQueue(test::TestCallbackWaiter* waiter,
                      std::unique_ptr<ReportQueue> report_queue) {
    report_queue_ = std::move(report_queue);
    if (waiter) {
      waiter->Signal();
    }
  }
  base::OnceCallback<void(std::unique_ptr<ReportQueue>)> GetReportQueueSetter(
      test::TestCallbackWaiter* waiter) {
    return base::BindOnce(&MockReportQueueConsumer::SetReportQueue,
                          weak_factory_.GetWeakPtr(), base::Unretained(waiter));
  }
  ReportQueue* GetReportQueue() const { return report_queue_.get(); }

 private:
  std::unique_ptr<ReportQueue> report_queue_;
  base::WeakPtrFactory<MockReportQueueConsumer> weak_factory_{this};
};

class ReportQueueFactoryTest : public ::testing::Test {
 protected:
  ReportQueueFactoryTest() = default;

  void SetUp() override {
    consumer_ = std::make_unique<MockReportQueueConsumer>();
    provider_ = std::make_unique<NiceMock<MockReportQueueProvider>>();
    report_queue_provider_test_helper::SetForTesting(provider_.get());
  }

  void TearDown() override {
    report_queue_provider_test_helper::SetForTesting(nullptr);
    task_environment_.RunUntilIdle();  // Drain remaining scheduled tasks.
  }

  const Destination destination_ = Destination::UPLOAD_EVENTS;
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  std::unique_ptr<MockReportQueueConsumer> consumer_;
  std::unique_ptr<MockReportQueueProvider> provider_;
};

TEST_F(ReportQueueFactoryTest, CreateAndGetQueue) {
  // Initially the queue must be an uninitialized unique_ptr
  EXPECT_FALSE(consumer_->GetReportQueue());
  {
    test::TestCallbackAutoWaiter set_waiter;
    ReportQueueFactory::Create(EventType::kDevice, destination_,
                               consumer_->GetReportQueueSetter(&set_waiter));
    EXPECT_CALL(*provider_.get(), OnInitCompletedMock()).Times(1);
    provider_->ExpectCreateNewQueueAndReturnNewMockQueue(1);
  }
  // We expect the report queue to be existing in the consumer.
  EXPECT_TRUE(consumer_->GetReportQueue());
}

TEST_F(ReportQueueFactoryTest, CreateQueueWithInvalidConfig) {
  // Initially the queue must be an uninitialized unique_ptr
  EXPECT_FALSE(consumer_->GetReportQueue());
  ReportQueueFactory::Create(EventType::kDevice,
                             Destination::UNDEFINED_DESTINATION,
                             consumer_->GetReportQueueSetter(nullptr));
  // Expect failure before it gets to the report queue provider
  EXPECT_CALL(*provider_.get(), OnInitCompletedMock()).Times(0);
  // We do not expect the report queue to be existing in the consumer.
  EXPECT_FALSE(consumer_->GetReportQueue());
}

TEST_F(ReportQueueFactoryTest, CreateAndGetQueueWithValidReservedSpace) {
  EXPECT_FALSE(consumer_->GetReportQueue());
  {
    test::TestCallbackAutoWaiter set_waiter;
    ReportQueueFactory::Create(EventType::kDevice, destination_,
                               consumer_->GetReportQueueSetter(&set_waiter),
                               /*reserved_space=*/12345L);
    EXPECT_CALL(*provider_.get(), OnInitCompletedMock()).Times(1);
    provider_->ExpectCreateNewQueueAndReturnNewMockQueue(1);
  }
  // We expect the report queue to be existing in the consumer.
  EXPECT_TRUE(consumer_->GetReportQueue());
}

TEST_F(ReportQueueFactoryTest, CreateQueueWithInvalidReservedSpace) {
  // Initially the queue must be an uninitialized unique_ptr
  EXPECT_FALSE(consumer_->GetReportQueue());
  ReportQueueFactory::Create(EventType::kDevice, destination_,
                             consumer_->GetReportQueueSetter(nullptr),
                             /*reserved_space=*/-1L);
  // Expect failure before it gets to the report queue provider
  EXPECT_CALL(*provider_.get(), OnInitCompletedMock()).Times(0);
  // We do not expect the report queue to be existing in the consumer.
  EXPECT_FALSE(consumer_->GetReportQueue());
}

TEST_F(ReportQueueFactoryTest, CreateSpeculativeQueue) {
  // Mock internal implementation to use a MockReportQueue
  provider_->ExpectCreateNewSpeculativeQueueAndReturnNewMockQueue(1);
  const auto report_queue = ReportQueueFactory::CreateSpeculativeReportQueue(
      EventType::kDevice, destination_);
  EXPECT_THAT(report_queue, NotNull());
}

TEST_F(ReportQueueFactoryTest, CreateSpeculativeQueueWithValidReservedSpace) {
  // Mock internal implementation to use a MockReportQueue
  provider_->ExpectCreateNewSpeculativeQueueAndReturnNewMockQueue(1);
  const auto report_queue = ReportQueueFactory::CreateSpeculativeReportQueue(
      EventType::kDevice, destination_,
      /*reserved_space=*/12345L);
  EXPECT_THAT(report_queue, NotNull());
}

TEST_F(ReportQueueFactoryTest, CreateSpeculativeQueueWithInvalidReservedSpace) {
  const auto report_queue = ReportQueueFactory::CreateSpeculativeReportQueue(
      EventType::kDevice, destination_,
      /*reserved_space=*/-1L);
  EXPECT_THAT(report_queue, IsNull());
}

TEST_F(ReportQueueFactoryTest, CreateSpeculativeQueueWithInvalidConfig) {
  const auto report_queue = ReportQueueFactory::CreateSpeculativeReportQueue(
      EventType::kDevice, Destination::UNDEFINED_DESTINATION);
  EXPECT_THAT(report_queue, IsNull());
}

// Tests if two consumers use the same provider and create two queues.
TEST_F(ReportQueueFactoryTest, SameProviderForMultipleThreads) {
  auto consumer2 = std::make_unique<MockReportQueueConsumer>();
  EXPECT_FALSE(consumer_->GetReportQueue());
  EXPECT_FALSE(consumer2->GetReportQueue());
  {
    test::TestCallbackAutoWaiter set_waiter;
    set_waiter.Attach();
    ReportQueueFactory::Create(EventType::kDevice, destination_,
                               consumer_->GetReportQueueSetter(&set_waiter));
    ReportQueueFactory::Create(EventType::kUser, destination_,
                               consumer2->GetReportQueueSetter(&set_waiter));
    EXPECT_CALL(*provider_.get(), OnInitCompletedMock()).Times(1);
    provider_->ExpectCreateNewQueueAndReturnNewMockQueue(2);
  }
  // We expect the report queue to be existing in the consumer.
  EXPECT_TRUE(consumer_->GetReportQueue());
  // And for the 2nd consumer
  EXPECT_TRUE(consumer2->GetReportQueue());
}

}  // namespace reporting
