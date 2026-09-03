// Copyright 2026 The Cobalt Authors. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "cobalt/browser/resource_scheduler/cobalt_adaptive_resource_scheduler.h"

#include <memory>

#include "base/test/task_environment.h"
#include "cobalt/browser/resource_scheduler/cobalt_resource_throttle.h"
#include "net/base/request_priority.h"
#include "services/network/public/cpp/resource_request.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/loader/url_loader_throttle.h"

namespace cobalt {
namespace {

class MockThrottleDelegate : public blink::URLLoaderThrottle::Delegate {
 public:
  MOCK_METHOD(void, CancelWithError, (int, std::string_view), (override));
  MOCK_METHOD(void, Resume, (), (override));
};

class CobaltAdaptiveResourceSchedulerTest : public ::testing::Test {
 protected:
  void SetUp() override {
    scheduler_ = CobaltAdaptiveResourceScheduler::GetInstance();
    scheduler_->SetSettleDelayForTesting(base::Milliseconds(50));
    scheduler_->SetScrollState(false);
  }

  void TearDown() override { scheduler_->SetScrollState(false); }

  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  CobaltAdaptiveResourceScheduler* scheduler_ = nullptr;
};

TEST_F(CobaltAdaptiveResourceSchedulerTest, PassThroughWhenIdle) {
  network::ResourceRequest request;
  request.destination = network::mojom::RequestDestination::kImage;
  request.priority = net::RequestPriority::LOW;

  auto throttle = CobaltResourceThrottle::MaybeCreate(request);
  ASSERT_TRUE(throttle);

  bool defer = false;
  throttle->WillStartRequest(&request, &defer);
  EXPECT_FALSE(defer);
  EXPECT_FALSE(throttle->is_deferred());
}

TEST_F(CobaltAdaptiveResourceSchedulerTest, DefersLowPriorityDuringScrolling) {
  scheduler_->OnUserInteraction(22);  // DPAD_RIGHT
  EXPECT_TRUE(scheduler_->IsScrolling());

  network::ResourceRequest request;
  request.destination = network::mojom::RequestDestination::kImage;
  request.priority = net::RequestPriority::LOW;

  auto throttle = CobaltResourceThrottle::MaybeCreate(request);
  ASSERT_TRUE(throttle);

  MockThrottleDelegate delegate;
  throttle->set_delegate(&delegate);

  bool defer = false;
  throttle->WillStartRequest(&request, &defer);
  EXPECT_TRUE(defer);
  EXPECT_TRUE(throttle->is_deferred());
  EXPECT_EQ(scheduler_->GetDeferredCount(), 1u);

  // Expect Resume() when timer expires.
  EXPECT_CALL(delegate, Resume()).Times(1);
  task_environment_.FastForwardBy(base::Milliseconds(60));

  EXPECT_FALSE(scheduler_->IsScrolling());
  EXPECT_FALSE(throttle->is_deferred());
  EXPECT_EQ(scheduler_->GetDeferredCount(), 0u);
}

TEST_F(CobaltAdaptiveResourceSchedulerTest, NeverDefersCriticalResources) {
  scheduler_->OnUserInteraction(22);  // DPAD_RIGHT
  EXPECT_TRUE(scheduler_->IsScrolling());

  // Script request should not be deferred
  network::ResourceRequest script_request;
  script_request.destination = network::mojom::RequestDestination::kScript;
  script_request.priority = net::RequestPriority::HIGHEST;

  auto throttle = CobaltResourceThrottle::MaybeCreate(script_request);
  ASSERT_TRUE(throttle);

  bool defer = false;
  throttle->WillStartRequest(&script_request, &defer);
  EXPECT_FALSE(defer);
  EXPECT_FALSE(throttle->is_deferred());
}

}  // namespace
}  // namespace cobalt
